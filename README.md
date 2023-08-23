# Flash Jetson

1. `git clone -b releases/flash_l4t https://github.com/uzuna/jetson-imx708.git && cd jetson-imx708`
2. fix settings(Optional)
   1. fix L4T Version in Makefile. e.g. `MINOR:=3.1` -> `MINOR:=4.1`
   2. fix hostname, user and password.
   3. fix target BOARD
3. Connect Jetson in ForceRecovery Mode
4. `make all`
