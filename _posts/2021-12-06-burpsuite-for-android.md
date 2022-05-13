---
layout: post
title: "Setting up Burpsuite for Android Pentesting"
author: "BoT"
tags: "mobile"
---

Everyone knows that when it comes to penetration testing, [Burpsuite by PortSwigger](https://portswigger.net/burp/communitydownload) is our best friend. While there are tons of tutorials out there that guides one through the process of configuring their go-to browser to play well with Burpsuite, what about mobile applications? How does one get Burpsuite to intercept requests from an Android APK of their choosing?

As someone new to mobile audits, this was a question that stumped me too. So here's a quick and dirty guide on how to set up Burpsuite for mobile penetration testing purposes!

## Step 1: Install Prerequisite Tools

- Install Android Studios from the [official website](https://developer.android.com/studio)

  If you are using ZSH as your preferred terminal, make sure that `~/.zshrc` is updated with the necessary environment variables

  ```bash
  # Set $PATH variable to zsh profile
  vim .zshrc
    export ANDROID_HOME=$HOME/Library/Android/sdk
    export PATH=$PATH:$ANDROID_HOME/emulator
    export PATH=$PATH:$ANDROID_HOME/platform-tools
    export PATH=$PATH:$ANDROID_HOME/tools
    export PATH=$PATH:$ANDROID_HOME/tools/bin
    export PATH=$PATH:$ANDROID_HOME/build-tools/31.0.0

  # Update profile without exiting from the terminal
  source .zshrc
  ```

  In Android Studios, create the following virtual devices:

  | Name        | Purpose                               | Specification                                      |
  | ----------- | ------------------------------------- | -------------------------------------------------- |
  | `Playstore` | w Google Play, to download APK        | Nexus 5 API 29, Android 10.0 with Google Play, x86 |
  | `Rooted`    | w/o Google Play, a rooted environment | Nexus 5 API 23, Android 6.0, x86                   |

- Install `jadx`, `apktool` and `objection` via [Homebrew](https://brew.sh/)

  ```bash
  # Install jadx and apktool via Homebrew
  brew install jadx
  brew install apktool

  # Install objection via Python package manager
  pip3 install frida-tools
  pip3 install objection
  ```

## Step 2: Configure Android Emulator and Burpsuite

- At `rooted` emulator, click on the 3 dots to access the emulator settings.

  Navigate to `Settings > Proxy > Manual Proxy Configuration` and input the following:

  **Hostname:** 127.0.0.1 \
  **Port number:** 8082

- At Burpsuite, under `Proxy > Options > Proxy Listener > Add > Binding`, add the following:

  **Bind to port:** 8082 \
  **Bind to address:** All interfaces

- At Brupsuite, under `Proxy > Options > Proxy Listener > Export CA certificate`

  **Select** `Certificate in DER format` \
  **Export file** as `Burpsuite.CER`

- Drag and drop `Brupsuite.CER` into `rooted` emulator and install it under `Settings > Credentials Storage > Install from SD Card`

  **Certificate name:** Burpsuite \
  **Certificate use:** VPN and Apps

  Check if traffic from browser within `rooted` emulator is intercepted by Burpsuite, if yes, configuration is done correctly 🙌

## Step 3: Install Frida into `rooted` Emulator

- Install `frida-server` from [official GitHub release page](https://github.com/frida/frida/releases)

  As the `rooted` emulator is `x86` architecture (as confirmed via `adb shell getprop ro.product.cpu.abi`), `frida-server-15.1.12-android-x86.xz` the ideal version to download

  ```bash
  # Download and install Frida Server to emulator (Rooted)
  unxz frida-server-15.1.12-android-x86.xz
  adb push frida-server-15.1.12-android-x86 /data/local/tmp/
  adb shell "chmod 755 /data/local/tmp/frida-server-15.1.12-android-x86"
  adb shell "/data/local/tmp/frida-server-15.1.12-android-x86 &"
  ```

  Verify that Frida is correctly interacting with `rooted` emulator.

  ```bash
  adb devices -l
  frida-ps -Uai
  ```

  If `frida-ps` returns a list of packages with similar naming conventions as `com.android.x`, you're on the right track 🎉

## Step 4: Download and Install APK

- Use `Playstore` emulator to download chosen APK as per normal

- Extract APK to local computer via `adb pull`

  ```bash
  # Ensure that emulator (Playstore) is running
  adb shell "pm list packages | grep <APK-NAME>"
  adb shell "pm path <APK-PACKAGE-NAME>"
  adb pull <APK-PACKAGE-PATH> pulled.apk
  ```

- Patch APK with `objection` and start the `rooted` emulator

  ```bash
  # If there's error during this process, you'll have to manually patch the APK
  # See https://koz.io/using-frida-on-android-without-root/
  objection patchapk --source pulled.apk
  ```

- Install APK into `rooted` emulator and disable SSL pinning via `objection`

  ```bash
  # Ensure that emulator (Rooted) is running
  adb install patched.apk
  frida-ps -Uai | grep <APK-NAME>
  objection explore --gadget "<APK-NAME>" explore
  android sslpinning disable
  ```

  You should now be able to see APK's traffic intercepted at Burpsuite 😎
