---
layout: post
title: "Security Automation at Scale #2 - Escalating Impact"
author: "BoT"
tags: "bugbounty"
excerpt_separator: <!--more-->
---

AKA how to escalate from simple authentication bypass bug to dumping Personal Identifiable Information.

<!--more-->

Yep, you read that right.

If you have just joined us, do check out [part one](../2022-05-15/security-automation-at-scale) of this series where we explored how we detected authentication bypass bug at scale. While there's actually no automation from this point onwards, let's just keep the title as it is, yeah?

## Part 1: Authentication Bypass due to Weak Regex Expression

From our previous post, we have established that there exist companies with multiple subsidiaries (each with different domains) under their umbrella that uses Google OAuth as their primary set of first-layer authorization controls. Unfortunately, due to the sheer amount of domain prefixes they have to cater to, the regex expression used to ensure login-able email addresses are scoped to company-owned domains were weak.

Thanks to our automation efforts, however, we have netted portal X, an admin dashboard that was intended to only allow emails of the following domains to login:

- `@companyA1.com`
- `@companyA2.com`
- `@companyA3.com`

This portal was vulnerable to authentication bypass bug due to weak regex expression which allows attackers to create tampered Google email addresses such as `@companyA1.com.attacker.com` and successfully login even without an account.

In the case of portal X, while we were indeed able to login to the dashboard, no meaningful data were exposed as the backend API endpoint had proper authorization mechanism in place.

As it is, the impact of this bug is quite limited. By HackerOne standards, it would have been `Low` severity at best, which had me thinking -- are **_all_** endpoints similarly protected?

## Part 2: Identifying Endpoints with Broken Access Controls

Without the source code, one way to figure out what endpoints exists in the application would be to review the application's JavaScript chunk file found in the `Networks` and/or `Sources` tab of the browser inspector. While it is not the most efficient method, it does reduce the workload (and eyestrain) significantly.

At this stage, I typically use a combination of tools to help me with my endeavor:

1. Automation via [LinkFinder](https://github.com/GerbenJavado/LinkFinder)
   ```
   python linkfinder.py -i https://██████████████████.com/static/js/main.████████.chunk.js -o cli
   ```
2. Manually review with VSCode (RIP my eyes 😵)

   Where after downloading and inspecting the file, I collate relevant search terms to grep for. Some regex expressions I tend to use are:

   ```
   fetch\((.*?)\)
   method:"get"
   authorization:"bearer
   ```

And oh, fun fact, it is also possible to find tokens/secrets in chunk files like this. Do check out [this post](../2021-12-22/finding-sensitive-strings) for the niffy script I have written!

Anyway, after collecting the list of endpoints, I then used Burpsuite Intruder to mass test for response that are either 2xx in nature, or has abnormal response length.

All in all, portal X has ~10 endpoints with insufficient and/or missing authorization mechanisms that allows attacker to directly query for data despite the dashboard showing that they have no such permissions.

With this, the severity of the report would have risen to `Medium` tier, but is it possible to go even higher?

## Part 3: Escalating Impact by Chaining Vulnerabilities

The answer is yes.

In fact, after exploring what the endpoints offers, three of them in particular could be chained together to allow enumeration and downloading of all files within the portal's S3 buckets.

On a high-level, here's how it works:

1. Identify buckets available in portal
   ```
   GET /███████/get_buckets HTTP/2
   Host: ██████████████████.com
   ```
2. Identify files stored in buckets
   ```
   GET /███████/get_objects?bucketName=███████ HTTP/2
   Host: ██████████████████.com
   ```
3. Download files from buckets
   ```
   GET /███████/<bucket_name>/<file_name>.csv HTTP/2
   Host: ██████████████████.com
   ```

As luck would have it, the drained S3 buckets contains tons of PIIs. Couple with the fact that multiple sites of different top-level domains with the same hostname (e.g. `@companyA1.vn`, `@companyA1.tw`) are actually served by the same codebase, we easily obtained ~5 regions worth of PIIs via the same vulnerability chain.

That is lots and lots of PIIs -- so yes, the severity of this report is now `High`.

Wooho 🤑
