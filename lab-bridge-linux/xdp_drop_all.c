/* Compile with:
 *  clang -O2 -target bpf -c xdp_drop_all.c -o xdp_drop_all.o
 *
 * Install with:
 *  ip link set dev erspan1 xdp obj xdp_drop_all.o sec .text
 */

#include <linux/bpf.h>

int main()
{
    return XDP_DROP;
}
