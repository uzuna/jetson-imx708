# IMX708 kernel module for Jetson Orin Nano

このリポジトリは[Raspberry Pi Camera Module 3](https://www.raspberrypi.com/products/camera-module-3/)のラズベリーパイ向け実装をJetson Orin Nano 開発者キット向けに部分移植したものです。

完全な移植ではなく、実験以上の用途に適するものではありません。
また[解説記事](https://qiita.com/uzuna/items/d80418f910fe18e59645)を用意していますので参照ください。


## How to build

```sh
make setup
make build_kernel
make apply_patch
make build_dtb
make build_modules
```

## How to check on Jetson

1. Jetpack 6.0 DPをセットアップ済みの **Jetson Orin Nano DevKit** を準備し、CAM1にIMX708を接続します
2. Jetsonに `~/imx708` ディレクトリを作ります
3. Jetsonのホスト名を`JETSON_TARGET`に設定します`export JETSON_TARGET=<ip_addr or jetson_hostname>` 
4. ホストPCから`make cp` でファイルを転送します

1. Jetsonにsshして
2. `make overlay`でJetsonIOから設定します
   1. `Configure Jetson 24pin CSI Connector`
   2. `Configure for compatible hardware`
   3. `Camera IMX708 Dual`
   4. `Save pin changes`
   5. `Save and reboot to reconfigure pins`
   6. rebootを待つ
3. `make insmod`で`nv_imx708.ko`をロードする
4. `make check.0`で映像を取得する


## How to flash

以下でflashできます。


```sh
export HOSTNAME=<your jetson hostname>

make -C bsp-flash pre-flash
# Connect Jetson orin nano with RecoveryMode
make -C bsp-flash flash
```

### boot-up and ssh expose待ちで先に進められない場合

```console
***************************************
*                                     *
*  Step 3: Start the flashing process *
*                                     *
***************************************
Waiting for target to boot-up...
Waiting for target to boot-up...
```

HostのdmesgにUSBの接続エラーが出ている場合は、電源を切らずにUSBケーブルを再接続してください。  
JetsonのUSBデバイスモードの設定が不十分な状態で接続を拒否しているのが接続できない理由なので、再接続をして正しく認識させます。

```console
$ sudo dmesg
...
usb usb4-port2: Cannot enable. Maybe the USB cable is bad?
...
```
