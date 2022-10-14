---
layout: post
title: "Achieving Persistence with SQL Injection"
author: "BoT"
tags: "bugbounty"
excerpt_separator: <!--more-->
---

A tale of SQL injection (in 2022!) and the journey of making myself an admin for the LOLs 🥴

<!--more-->

Back during my undergraduate days, I remembered one of my professors off-handedly commented that there had been so much awareness over SQLi that it would be hard to find one in the wild.

Well, apparently not.

## Background

The story starts when I was checking out new assets flagged by my trusty authentication bypass tool.

As established in my [series on security automation](../2022-05-15/security-automation-at-scale), the affected target, a financial dashboard, allows non-company email addresses to login via Google OAuth. In fact, I could login with any `@gmail.com` account!

While no meaningful data were visible from the dashboard, I was undaunted as it may be possible for endpoints to neglect checking for authentication. With that in mind, I turned to look into the various JavaScript chunk files loaded during the login process.

Learning from my previous (torturous) experience with chunk files, instead of slowly looking at the file and attempt to decipher what was needed -- query parameters, JSON structure for POST, etc -- I opt to just say _"fuck it"_ and collate a list of endpoints via regex; dump the list into Burpsuite; set Intruder to run the payload as GET request regardless of its intended HTTP method.

The best lazy decision of my life.

## Part 1: Unauthenticated Download of Spreadsheet

Lucky for me, there was a request which returned results -- to be specific, an empty excel spreadsheet. As it is, this finding would have been triaged as `Low`, or even `Information` as I wouldn't be able to articulate what impact an empty spreadsheet could have.

Emboldened by my partial success however, I decided to reference back to the chunk file where this particular endpoint was defined and it seems to suggest that the need for query parameters.

Interesting.

Weirdly enough, the endpoint returned with a `500 Internal Server Error` when I added one of defined the parameters. Poking more into the code snippet revealed that for some reason, the endpoint requires either _none_ or _all_ of its query parameters included to generate the spreadsheet.

Unfortunately, solving this puzzle didn't yield any different results -- it still returns an empty excel spreadsheet -- because I didn't had the right values for the query parameters.

Grr 😡

Frustrated, I decided to add `'` (a quote) at one of the parameters for LOL sake and it returned a `500 Internal Server Error`.

Could this be an SQLi?

## Part 2: SQL Injection in Query Parameters

Spoiler alert, it totally was!

The first payload I tried was `123) OR 1=1) --` and to my excitement, I got back a `200 OK` response and a freshly downloaded spreadsheet.

Ohhhh yeah! Time to dump the entire database!

Instead of using automated tools like `sqlmap`, I decided to manually craft out the necessary payloads so to minimize the chances of triggering any alerts. Do note that these payload were all adapted from PortSwigger Web Academy [SQL Injection](https://portswigger.net/web-security/sql-injection) chapter, so do check them out if you are new to SQLi.

Anyway, here's an overview of what I did:

1.  Check number of columns supported by SQLi

    ```
    ') order by 6-- -
    ```

2.  Find list of tables

    ```
    ') union select table_name,'','','','' from information_schema.tables-- -
    ```

3.  Find columns of a table

    ```
    ') union select column_name,'','','','' from information_schema.columns where table_name='<TABLE_NAME>'-- -
    ```

4.  Dump data from a table

    ```
    ') union select <COLUMN_1>,<COLUMN_2>,<COLUMN_3>,<COLUMN_4>,<COLUMN_5> from <TABLE_NAME>-- -
    ```

One annoying thing about SQLi this application is that the results are reflected only in the downloaded spreadsheets. This meant that I had to open up each one of them for every payload I attempted. I'll admit I was so exasperated that I ended up automating this process through Burpsuite by dumping the list of payload into Intruder and letting it work its magic.

Automation FTW 🤓

## Part 3: Escalating Privileges to Admin

At this stage, the finding is actually good enough for a `High` severity rating, but I wanted to do more. As I trawled through the trove of dumped data, I suddenly realized one thing -- I could actually achieve persistence by elevating my humble `@gmail.com` account to an admin!

This is possible because I could infer what user roles were the most powerful from results of `user_roles`, a table that maps an account's email address with a user role. Of course, I would need to fire an update payload to amend my user role, but I opted not to as I didn't want to risk accidentally overwriting data if I were to mess up.

This brought me to my next question:

> Is there other ways to obtain admin privileges?

The answer dawned on me when I noticed `social_auth_usersocialauth`, a table generated by `python-social-auth` -- a library used by the application to handle OAuth logins -- that contained email addresses and Google tokens of `ya29.xxx` format.

Remember that `user_roles` table?

Yeah, me too.

Now that I know which email addresses has the most powerful user role, I simply swapped out my authorization token for their token via Burpsuite's Match and Replace function.

And there we have it, god mode 🤪

## Conclusion

As it turns out, this particular financial dashboard was just one component of out a suite of interlaced system. Not surprisingly, a large majority of them were plagued by same set of vulnerabilities. One memorable application actually had an IDOR which allowed unauthenticated attackers to download nearly 18k files. Yes, you read that right, 18,000 of them!

Talk about horrifying.

That said, this was definitely my biggest finding of the year (in fact, my biggest finding ever) and I am super happy about it. Here's to more critical/high findings in the future 🥂
