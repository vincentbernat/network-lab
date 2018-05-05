// Simulate many clients toward a given set of IP services. Provide a
// small graph on screen to show when errors occur. IPs of the clients
// should be on the loopback interface. The service IPs are provided
// on the command-line.
package main

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"math/rand"
	"net"
	"net/http"
	"net/url"
	"os"
	"os/exec"
	"os/signal"
	"sync"
	"syscall"
	"time"
)

const (
	parallel = 200 // number of clients
	interval = 500 // maximum interval between reads
)

func getSources() []net.IP {
	cmd := exec.Command("ip", "-json", "addr", "list", "dev", "lo")
	stdout, err := cmd.StdoutPipe()
	if err != nil {
		log.Panicf("while setting stdout output: %+v", err)
	}
	if err := cmd.Start(); err != nil {
		log.Panicf("while executing `ip addr list': %+v", err)
	}
	var interfaces []struct {
		AddrInfo []struct {
			Local     net.IP
			PrefixLen int
			Scope     string
		} `json:"addr_info"`
	}
	if err := json.NewDecoder(stdout).Decode(&interfaces); err != nil {
		log.Panicf("while parsing output of `ip addr list': %+v", err)
	}
	if err := cmd.Wait(); err != nil {
		log.Panicf("while waiting for `ip addr list': %+v", err)
	}

	results := []net.IP{}
	for _, intf := range interfaces {
		for _, addrinfo := range intf.AddrInfo {
			if addrinfo.Scope == "global" {
				results = append(results, addrinfo.Local)
			}
		}
	}

	return results
}

func main() {
	sources := getSources()
	destinations := []net.TCPAddr{}
	for _, arg := range os.Args[1:] {
		addr, err := net.ResolveTCPAddr("tcp", arg)
		if err != nil {
			log.Panicf("while solving address %s: %+v", arg, err)
		}
		destinations = append(destinations, *addr)
	}

	var wg sync.WaitGroup
	ctx, cancel := context.WithCancel(context.Background())
	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	result := make(chan bool, 10)

	// Handle results
	go func() {
		var successes, failures uint
		var c string
		for {
			ok := <-result
			if ok {
				successes++
			} else {
				failures++
			}
			if successes+failures != 8 {
				continue
			}
			if successes == 0 {
				c = "\033[1;31m_\033[0;0m"
			} else if successes == 8 {
				c = "\033[1;32m⣿\033[0;0m"
			} else {
				c = fmt.Sprintf("\033[1;31m%c\033[0;0m",
					rune(0x2800+(1<<8)-(1<<failures)))
			}
			fmt.Print(c)
			failures = 0
			successes = 0
		}
	}()

	buffer := make([]byte, 1024*1024) // Shared, nobody reads it
	for i := 0; i < parallel; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for {
				select {
				case <-ctx.Done():
					return
				default:
				}

				source := sources[rand.Intn(len(sources))]
				destination := destinations[rand.Intn(len(destinations))]
				if source.To4() != nil && destination.IP.To4() == nil {
					continue
				}
				if source.To4() == nil && destination.IP.To4() != nil {
					continue
				}

				client := &http.Client{
					Transport: &http.Transport{
						DialContext: (&net.Dialer{
							LocalAddr: &net.TCPAddr{IP: source},
							Timeout:   2 * time.Second,
							KeepAlive: time.Second,
							DualStack: true,
						}).DialContext,
					},
				}
				url := url.URL{
					Scheme: "http",
					Path:   "10M",
				}
				if destination.IP.To4() != nil {
					url.Host = fmt.Sprintf("%s:%d", destination.IP, destination.Port)
				} else {
					url.Host = fmt.Sprintf("[%s]:%d", destination.IP, destination.Port)
				}

				// Do request and read answer slowly
				req, err := http.NewRequest("GET", url.String(), nil)
				if err != nil {
					result <- false
					continue
				}
				req = req.WithContext(ctx)
				resp, err := client.Do(req)
				if err != nil {
					result <- false
					continue
				}
				if resp.StatusCode != 200 {
					result <- false
					resp.Body.Close()
					continue
				}
				total := int64(0)
			OUTER:
				for {
					select {
					case <-ctx.Done():
						break OUTER
					case <-time.After(time.Duration(rand.Intn(interval)) * time.Millisecond):
					}
					n, err := resp.Body.Read(buffer)
					total += int64(n)
					if err == io.EOF {
						result <- true
						break
					}
					if err == context.Canceled {
						break
					}
					if err != nil {
						result <- false
						break
					}
					if rand.Intn(100) >= 95 {
						result <- true
						break
					}
				}
				resp.Body.Close()
			}
		}()
	}

	<-quit
	cancel()
	wg.Wait()
	log.Printf("bye...")
}
