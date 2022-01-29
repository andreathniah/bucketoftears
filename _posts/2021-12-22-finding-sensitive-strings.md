---
layout: post
title: "Finding Sensitive Strings in APKs"
author: "BoT"
tags: "mobile"
excerpt_separator: <!--more-->
---

Or in any files, actually 🙈

<!--more-->

Mobile pentesting is scary. There are just so many things to look at, and if you are a total beginner like myself, you'll feel as lost as I am. Before we delve into the monstrosity known as [OWASP MSTG](https://mobile-security.gitbook.io/mobile-security-testing-guide/), let's first look into the easy wins -- hardcoded keys.

More often than not, the first place to look for such keys would be in `AndroidManifest.xml` and `strings.xml`, what about other files in the APK? Of course, one can simply open the source code in their preferred IDE and manually search for the keys via the search function, but it certainly is time consuming.

Wouldn't it be great if there is a way to automate this process and efficiently sniff out needles in the haystack? Or at the very least, narrow down the findings to several strings to verify? Well, that's what I thought too.

### Automating the Hunt

Yes, there are many tools out there that could do the very same thing, but I'm a stubborn fool who likes to re-invent the wheels so here's the [yet-to-be-named-script](https://github.com/andreathniah/tips-and-tricks/blob/master/scripts/find-high-entropy-strings.py) that leverages on entropy checks and plain old regex checks to find sensitive strings. I'll be honest here, this is essentially a fork of [truffleHog](https://github.com/trufflesecurity/truffleHog) with modifications to support consumption of files-based contents, plus a little extra stuff sprinkled here and there.

The various options available allows one to:

- Search for sensitive strings via entropy checks or regex patterns from:
  - A specified file
  - A specified URL's source code
  - All files within a specified directory
- Verbose mode to view full output and context of sensitive strings found

While this tool can be utilised on any platforms so long as the ingested data are from a file -- like JavaScript chunk files which this script was originally intended for -- limitation exists.

For instance, searches via entropy checks will almost always produce false positives. This is especially so if the source data is not properly formatted to multiple lines (`npx prettier --write` will be helpful here) or has legitimate high entropy strings like base64 images, automated classnames etc. Checks via regex performs better, but its constrained in the pre-defined patterns and whether it is accurate/versatile enough. As someone who has lots of trouble with constructing regex expression, this part is especially painful to get it right.

### Security Impact

That said, the security impact of any strings found are ultimately determined by what you can actually do with them. Say that the script churned out some Google Firebase API key, the question is: [are you able to use it to access misconfigured services?](../2022-01-14/finding-firebase-misconfigs) Without demonstration of concrete impact, most triage teams would close the report as `N/A` as issue is related to best-practices; or at best, `Informative` under Information Disclosure category which may not have a payout.

All in all, tools such as this merely helps one to narrow down the findings into bitesize actionables. Further analysis and exploration are always necessary to determine the true impact of such strings and whether their disclosure can truly be considered a security vulnerability.
