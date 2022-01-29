---
layout: post
title: "Leaked API Keys and Firebase Misconfigurations"
author: "BoT"
tags: "mobile"
excerpt_separator: <!--more-->
---

Okay, you reversed an APK and you found a couple of Google API keys, now what?

<!--more-->

Good question. When it comes to mobile penetration testing, the lowest hanging fruits are the Gogole API keys used for Firebase projects. Unfortunately, the [official Firebase documentation](https://firebase.google.com/docs/projects/api-keys) highlights:

> _Unlike how API keys are typically used, API keys for Firebase services are **not** used to control access to backend resources; that can only be done with Firebase Security Rules [...] and App Check [...]_

This means that unless we can demostrate that the keys are used in misconfigured services, the security impact of such leaked keys are practically zero.

After [some digging](https://stackoverflow.com/questions/37482366/is-it-safe-to-expose-firebase-apikey-to-the-public), it seems like there is a way after all -- especially if user registrations via API keys are enabled. With a valid authentication token, an attacker can simply utilize the Firebase SDK to authenticate and run queries as a normal user. This effectively transforms the security weakness from a simple information disclosure to a potential improper access control issue.

With the question becoming: _"Are there any Firebase-related services that does not properly validate whether the user is authorized to take certain actions?"_ Here's a simple checklist I use to determine if the API key is overly permissble ([and yes, I wrote a script to automate this](https://github.com/andreathniah/tips-and-tricks/blob/master/scripts/check-firebase-misconfiguration.py)):

- Does the key allows unauthenticated write in Cloud Messaging?
- Does the key allows unauthenticated read/write in Cloud Firestore?
- Does the key allows unauthenticated read/write in Realtime Database?
- Does the key allows creation of new user account? If yes, does the authentication token:
  - Allows authenticated read/write in Realtime Database?
  - Allows authenticated file upload in Cloud Storage?

That said, illustrating the impact of these misconfigurations are rather hit and miss. Even if the authentication token is indeed capable of writing an arbitrary file, the triage team may deem the set of actions as intended behaviors if the token isn't able to read/write files belong to **other users**. At best, it would be regarded as a low severity report due to the lack of confidential data revealed in the said Firebase project.

All in all, don't get your hopes too high up, but do try your luck anyway!
