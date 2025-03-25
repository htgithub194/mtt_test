### Write a Ping program

* My ping program can run with ipv4 address or a domain.

* But can not run with IPV6, because my infrastructure network does not support IPV6. I can not do test with IPV6.
    
    * I do not know why. Just tried with debian's ping/ping6 command but it did not work.


* Here are some manual test:

```sh
# build 
gcc mtt_ping.c -o mtt_ping

# run
sudo ./mtt_ping  google.com
PING google.com (142.250.198.110):
64 bytes from 142.250.198.110: icmp_seq=1 ttl=64 rtt=62.243605 ms
64 bytes from 142.250.198.110: icmp_seq=2 ttl=64 rtt=62.504453 ms
64 bytes from 142.250.198.110: icmp_seq=3 ttl=64 rtt=62.642945 ms

=== 142.250.198.110 ping statistics ===
3 packets sent, 3 packets received, 0.000000% packet loss. Total time: 3187.929334 ms.

# run with -c
 sudo ./mtt_ping  google.com -c 10
PING google.com (142.250.198.46):
64 bytes from 142.250.198.46: icmp_seq=1 ttl=64 rtt=62.540096 ms
64 bytes from 142.250.198.46: icmp_seq=2 ttl=64 rtt=62.434698 ms
64 bytes from 142.250.198.46: icmp_seq=3 ttl=64 rtt=62.743619 ms
64 bytes from 142.250.198.46: icmp_seq=4 ttl=64 rtt=62.586745 ms
64 bytes from 142.250.198.46: icmp_seq=5 ttl=64 rtt=62.685335 ms
64 bytes from 142.250.198.46: icmp_seq=6 ttl=64 rtt=61.972730 ms
64 bytes from 142.250.198.46: icmp_seq=7 ttl=64 rtt=62.644319 ms
64 bytes from 142.250.198.46: icmp_seq=8 ttl=64 rtt=62.320819 ms
64 bytes from 142.250.198.46: icmp_seq=9 ttl=64 rtt=61.539805 ms
64 bytes from 142.250.198.46: icmp_seq=10 ttl=64 rtt=62.625692 ms

=== 142.250.198.46 ping statistics ===
10 packets sent, 10 packets received, 0.000000% packet loss. Total time: 10626.131707 ms.

# run with timeout 1sec, incase host unreachable
sudo ./mtt_ping 192.168.1.2 -c 5 -t 1
PING 192.168.1.2 (192.168.1.2):
From 192.168.1.2 Destination Host Unreachable
From 192.168.1.2 Time to live exceeded
From 192.168.1.2 Destination Host Unreachable
From 192.168.1.2 Destination Host Unreachable
From 192.168.1.2 Time to live exceeded

=== 192.168.1.2 ping statistics ===
5 packets sent, 0 packets received, 100.000000% packet loss. Total time: 8064.324914 ms.
```

### Problem1

* I found the bug in function *temp410*

* After a fix as bellow, the program can run to it's end.

```C
int temp410() {
	int x[5];
	// x[10] = 0x0;
	return x[4]; 
}
```

### Problem 2


* Because there is no source code. To detect the problem, I do the following steps:

    * Dump asm code:
    ```sh
    objdump -d problem2 > asm
    ```
    * Look in to the dumped asm code, and see that it's quite same with problem1.

    * Using GDB to place debug break

    * After several tries, I found the bug in function *temp4245*

    ```asm
    000000000001bacf <temp4245>:
   1bacf:	55                   	push   %rbp
   1bad0:	48 89 e5             	mov    %rsp,%rbp
   1bad3:	48 83 ec 20          	sub    $0x20,%rsp
   1bad7:	64 48 8b 04 25 28 00 	mov    %fs:0x28,%rax
   1bade:	00 00 
   1bae0:	48 89 45 f8          	mov    %rax,-0x8(%rbp)
   1bae4:	31 c0                	xor    %eax,%eax
   1bae6:	c7 45 08 00 00 00 00 	movl   $0x0,0x8(%rbp)
   1baed:	8b 45 f0             	mov    -0x10(%rbp),%eax
   1baf0:	48 8b 55 f8          	mov    -0x8(%rbp),%rdx
   1baf4:	64 48 33 14 25 28 00 	xor    %fs:0x28,%rdx
   1bafb:	00 00 
   1bafd:	74 05                	je     1bb04 <temp4245+0x35>
   1baff:	e8 3c 4a fe ff       	call   540 <__stack_chk_fail@plt>
   1bb04:	c9                   	leave
   1bb05:	c3                   	ret
    ```

* To fix this problem, I propose to replace the call to *temp4245* to *NOP*, because I do not know much about this kind of issue; I just try to bypass the crash.

* The steps are:

    * open the executed file in *radare2*
    
    * goto address 0xde0f

    * replace *NOP* to the call to *temp4245* 

    * Run the new executed file, no crash.

```sh
r2 -w problem2
s 0xde0f            # seek to addr
pd 10               # show 10 next instructions
wao+nop             # replace current instruction to NOP
wao+nop             # replace current instruction to NOP
wao+nop             # replace current instruction to NOP
q                   # quit
```

* Create patch file using *bsdiff*

```sh
# bsdiff old_f      new_f      out_patch_F
bsdiff problem2 edit_problem2 patch.bsdiff
```

* To apply the patch:

```sh
# bspatch old_f     out_F       patch_F
bspatch problem2 problem2_apply_fix patch.bsdiff
```