---
layout: post
title: "Deploying GUI Applications on Kubernetes"
author: "BoT"
tags: "devops"
excerpt_separator: <!--more-->
---

We have tons of tutorials online teaching us how to run GUI applications on Docker. But barely any of them talked about how to do so on Kubernetes.

<!--more-->

I know, I know, you must be wondering, why in the world would you need to run GUI applications using Kubernetes???? Believe it or not, I actually had a legitimate use-case for it and it was painful figuring out the _hows_ of running dockerized GUI applications at scale.

## Preresquites

So, first thing first, to run a GUI application, you'll first need to have a GUI environment. This means `/tmp/.X11-unix` must be available.

You might be tempted to start off with a Ubuntu Server 20.04, install UI into it, set up RDP, blah blah blah -- it's a hassle and things _will_ break if you configure them wrongly, but if you are a masochist (or a pro), by all means, go ahead.

That said, we'll be using Ubuntu Desktop 20.04 LTS as our OS for this tutorial.

## Instructions

### Step 1: Install Docker and Docker Compose

```bash
curl -fsSL https://get.docker.com -o get-docker.sh
sh get-docker.sh
sudo apt install python3-pip
pip3 install docker-compose
```

### Step 2: Spin Up Dockerized Firefox Container

This step mainly checks if the server is properly configured to allow dockerized GUI applications to run. Most issues on this step arises when `xhost` is not set to allow any host to connect to your server. For those seeking for more restrictive settings, do check [this StackOverflow post](https://stackoverflow.com/questions/28392949/running-chromium-inside-docker-gtk-cannot-open-display-0).

Note that you can also choose to supply hardcoded value for `DISPLAY` variable (e.g. `:0`)

```bash
echo $DISPLAY   # check which display is used -- usually :0
xhost +         # or xhost +local:docker
docker run --rm -e DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix aliustaoglu/firefox
```

### Step 3: Spin Up Customized Container

In this step, we'll be attempting to spin up dockerized Puppeteer in headful mode. Our folder directory looks like this:

```
✗ tree .
.
├── Dockerfile
├── docker-compose.yaml
└── index.js
```

Once the files are in place, we can simply build the container and spin them up with `docker-compose up --build`

#### `docker-compose.yaml`

```yaml
version: "3"
services:
  puppeteer-headful:
    build: .
    command: node /xbin/index.js
    environment:
      - DISPLAY
    volumes:
      - /tmp/.X11-unix:/tmp/.X11-unix
```

#### `Dockerfile`

```Dockerfile
FROM buildkite/puppeteer

COPY . /xbin
ENV NODE_PATH /usr/local/lib/node_modules

# Install dependencies required for GUI
RUN apt-get update &&\
apt-get install -yq gconf-service libasound2 libatk1.0-0 libc6 libcairo2 libcups2 libdbus-1-3 \
libexpat1 libfontconfig1 libgcc1 libgconf-2-4 libgdk-pixbuf2.0-0 libglib2.0-0 libgtk-3-0 libnspr4 \
libpango-1.0-0 libpangocairo-1.0-0 libstdc++6 libx11-6 libx11-xcb1 libxcb1 libxcomposite1 \
libxcursor1 libxdamage1 libxext6 libxfixes3 libxi6 libxrandr2 libxrender1 libxss1 libxtst6 \
ca-certificates fonts-liberation libappindicator1 libnss3 lsb-release xdg-utils wget \
xvfb x11vnc x11-xkb-utils xfonts-100dpi xfonts-75dpi xfonts-scalable xfonts-cyrillic x11-apps
```

#### `index.js`

```javascript
const puppeteer = require("puppeteer");

(async () => {
  const browser = await puppeteer.launch({
    headless: false,
    args: ["-no-sandbox", "-disable-setuid-sandbox"],
  });
  const page = await browser.newPage();
  await page.goto("https://example.com");
  await page.screenshot({ path: "example.png" });
  await browser.close();
})();
```

### Step 4: Craft YAML File for Customized Container

This step assumes that you will be using [Argo Workflow](https://argoproj.github.io/argo-workflows/) for delegation of tasks on Kubernetes cluster. For more information on setting up of Argo Workflow, check out [this blogpost](../2021-08-04/local-install-argo-workflow).

The contents in `argo.yaml` file are actually directly converted from the `docker-compose.yaml` file seen in the previous step. The only modifications were to:

- Add `nodeAffinity` to ensure GUI-related tasks are assigned to node with X11 installed
- Hardcode `DISPLAY` variable as it is impossible for Kubernetes to dynamically establish the value without extensive fiddling

Now that all the ingredients are in the frying pan, we can start cooking with `argo submit -n argo argo.yaml --watch`

_Andddd_ that's it! You have successfully deployed a dockerized GUI application via Kubernetes 🎉

#### `argo.yaml`

```yaml
apiVersion: argoproj.io/v1alpha1
kind: Workflow
metadata:
  generateName: headful-docker-
spec:
  affinity:
    nodeAffinity:
      requiredDuringSchedulingIgnoredDuringExecution:
        nodeSelectorTerms:
          - matchExpressions:
              - key: argo.role.gui
                operator: In
                values: ["true"]
  entrypoint: puppeteer-headful
  templates:
    - name: puppeteer-headful
      container:
        build: .
        env:
          - name: DISPLAY
            value: ":0"
        command: [node]
        args:
          - /xbin/index.js
        volumeMounts:
          - name: x11
            mountPath: /tmp/.X11-unix
      volumes:
        - name: x11
          hostPath:
            path: /tmp/.X11-unix
```

Wait a second, how in the world did you enroll the Ubuntu Desktop into an existing Kubernetes cluster in the first place?

Well... that is one huge step that deserves a post of its own, so that's for another time. Though if you are already knowledgeable with Kubernetes, this shouldn't be an issue.

## References

Transforming Ubuntu Server to Ubuntu Desktop

- [How to install Ubuntu with GUI on Digital Ocean for free?](https://systemweakness.com/how-to-install-ubuntu-with-gui-on-digital-ocean-for-free-12301f464a3)
- [Ubuntu Remote Desktop On Digital Ocean](https://jerrygamblin.com/2016/10/19/ubuntu-remote-desktop-on-digital-ocean/)

Running GUI Applications in Docker

- [Docker Container GUI Display](https://leimao.github.io/blog/Docker-Container-GUI-Display/)
- [Running a Docker Container with GUI on Mac OS](https://affolter.net/running-a-docker-container-with-gui-on-mac-os/)
- [Running GUI applications in Docker on Windows, Linux and Mac hosts](https://cuneyt.aliustaoglu.biz/en/running-gui-applications-in-docker-on-windows-linux-mac-hosts/)
- [Running Puppeteer as headful mode inside docker](https://sreejithmsblog.wordpress.com/2019/02/25/running-puppeteer-as-headful-mode-inside-docker/)

Deploying GUI Applications on Kubernetes

- [Kubernetes Pods - Inject environment variable from host node](https://stackoverflow.com/questions/60730229/kubernetes-pods-inject-environment-variable-from-host-node)
