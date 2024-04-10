# Linux sources for CanMV-K230 board

Fork based on [T-Head Linux fork][t-head], with patches from [Rémi
Denis-Courmont][remi-linux] (possible to build in [k230-boot][k230-boot]
environment) updated to SDK v1.4 from [CanMV vendor repo][canmv].

[t-head]: https://github.com/T-head-Semi/linux
[remi-linux]: https://code.videolan.org/Courmisch/linux
[k230-boot]: https://code.videolan.org/Courmisch/k230-boot/
[canmv]: https://github.com/kendryte/k230_sdk/tree/main/src/little/linux

T-Head repo has `linux-6.6-k230`, but it doesn't have all the drivers required
for fully working kernel.
