---
layout: post
title: "How an IDOR Vulnerability Leaked Tons of PIIs"
tags: "bugbounty"
author: "BoT"
excerpt_separator: <!--more-->
---

Responsible Disclosure. AKA Bug Bounty. AKA How I got 500 dollars richer with IDOR!

<!--more-->

## Bug Bounty... What!?

As student who only have had experience with deliberately vulnerable labs/machines, bug bounty was something I never envisioned myself to be capable of participating.

Ironically, my first 2 valid bugs were not something I've actively hunted for -- the vulnerabilities were just stuff that caught my eyes while I was utilizing the platforms. In fact, these were actually considered responsible disclosure as the platforms in question has no active bug bounty programs.

Since these vulnerabilities have been fixed, I thought that it would be a good idea to jot down about some stuff I've learned along the way. Following standards of disclosures such as this, all identifiable information of the affected platforms have been redacted.

## Insecure Direct Object Reference

Both reports noted in this posts suffered from the same class of vulnerability -- Insecure Direct Object Reference (IDOR) that led to circumvention of access controls.

No one can explain IDOR better than [PortSwigger](https://portswigger.net/web-security/access-control/idor), so I shall quote them here:

> _IDOR are a type of access control vulnerability that arises when an application uses user-supplied input to access objects directly._
>
> _Consider a website that uses the following URL to access the customer account page, by retrieving information from the back-end database:_
>
> _`https://insecure-website.com/customer_account?customer_number=132355`_
>
> _Here, the customer number is used directly as a record index in queries that are performed on the back-end database. If no other controls are in place, an attacker can simply modify the `customer_number` value, bypassing access controls to view the records of other customers. This is an example of an IDOR vulnerability leading to horizontal privilege escalation._

So yep, get ready to see some IDOR in action!

## Report #1

The first report affects a platform with over 90 million users in the community. Within this platform, there exists a functionality which allow users to block specific individuals from viewing their profile page. Blocked users will simply see default page -- think 404 page -- when navigated to the profile page in question.

Unfortunately, the API endpoint that powers this logic of determining whether the viewer is blocked suffers from IDOR. This led to a full database dump of the `account` table and exposed massive amount of sensitive information from parameters such as `name`, `password`, `realname`, `location`, `country` .

Yes, these are Personal Identifiable Information (PII). And yes, I got cool swags and a good amount of 💰💰💰 out of this report.

### Detailed Attack Scenario

**Reproduction Steps**

1. Login into the platform as `@Alice` and navigate to the profile page of a random user, `@Bob`, which the user is allowed to view.

   A closer look at the XHRs executed would reveal an interesting query `/api/█████?id=Alice&action=██████████` with a 200 response.

   ```json
   { "result": "SUCCESS", "█████": [], "██████████": "" }
   ```

2. Manipulate this query by changing `id` parameter to a non-existent user, `@Charlie`. A 400 response will be returned, informing me that the user specified is no longer a user.

   ```json
   { "result": "ERROR", "code": "███", "message": "Charlie is no longer a member." }
   ```

3. Digging further, I replaced `id` parameter with another random user that `@Alice` has access to, `@Dave`. Jackpot! Sensitive data was dumped in full clear-text 👀

   ```json
   { "result": "SUCCESS",
     "█████": [{
     		   ...
   		"ID": "█████",
   		"NAME": "██████████",
   		"PASSWORD": "████████████████████",
   		"EMAIL": "████████████████████",
   		"LOGINIP": "██████████",
   		"COUNTRY": "██",
   		"AGE": "███████"
   	}, { ... }, { ... } ],
     "██████████": ""
   }
   ```

Did anyone found it weird that the initial call with `@Bob` as the target returned an empty array, but `@Dave` returned a wealth of information? Turns out, users actually have different rights depending on their `status` – think Twitter's blue check -- and therefore, returns differing sets of account information within this vulnerable endpoint. In fact, one verified account I tested leaked **200+ sets of account information** within a single request! Pretty neat ain't it?!

## Report #2

The second report is based off a platform akin to a job portal. Just like platforms of this nature, it allows registered users to update their profiles with private, _verified_ information -- think of national ID, local addresses, etc.

These are obviously confidential information which would undoubtedly breach local data protection laws if leaked in large quantity. Unfortunately, the lack of proper access control allows any existing users to access personal information that does not belong to them via IDOR.

And no, I didn't get a bounty for this. I did snagged a _"Good Samaritan"_ badge from HackerOne thanks to this report though!

### Detailed Attack Scenario

Unlike the previous report, this platform has multiple affected endpoints.

- `/██████████.aspx/Get███ID`
- `/██████████.aspx/GetMyInfo`
- `/██████████.aspx/Get███InformationByID`
- `/██████████.aspx/Get███InformationBy██████`
- `/██████████.aspx/GetProfileImage`

I suspected data modification to be similarly possible via `/Update███Information`. However, I did not attempt to execute the POST request as it would have modified information of the unfortunate target.

It is also worth noting that users also can choose to import their details from a separate _national_ centralized database that not related to this asset via `/GetMyInfo` endpoint. This, however, was never confirmed as I didn't dare to test the endpoint with a target's account. _Nope, not gonna risk the fall out if something happens_ 🙅‍♀️

**Reproduction Steps**

1. Login into the platform as `@Alice` and navigate to her profile page. Here, 2 endpoints are being called in succession:

   `/Get███ID` which returns `@Alice` ID number, `123`, as the response; and\
   `/Get███InformationByID` which utilities the ID number and returns information related to `@Alice`

2. Edit and resend `/Get███InformationByID` request with another value, `122`, as the ID. Large amount of PII related to this user, `@Bob`, is retrieved.

   While analyzing the response returned, I noted a bunch of `base64` encoded information to be present within some of the parameters. Pushing them into online decoders such as [Base64 to PDF Converter](https://base64.guru/converter/decode/pdf), I had come to find _more_ information pertaining to `@Bob` within these files. _Oof._

I can't stress the amount of PII leaked by IDOR in this particular report. While report #1 returns multiple set of PII from different accounts, it is limited in the sense that you won't be able to [OSINT](https://portswigger.net/daily-swig/osint-what-is-open-source-intelligence-and-how-is-it-used) the affected user. Report #2 certainly takes the cake in that regard, especially since these PII are _legit_ and cannot be faked (its tied to your national identity number). Big yikes.

## Closing Thoughts

- Sites actually collect _hella lots of_ PIIs, maybe we should rethink the amount of personal information we share 🤔

- IDOR doesn't require complicated tools to identify them. All of my testing were done over Firefox browser by analyzing the XHRs at the network panel.

- Don't get carried away. Check your local laws before doing bug bounty and/or responsible disclosure. In my case, testing the centralized server at Report #2 would have definitely gotten me into trouble if I had messed up something -- unintentionally or otherwise.

- No, I haven't had success with any Bug Bounties since these 2 IDORs. Sad.
