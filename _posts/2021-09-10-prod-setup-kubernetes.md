---
layout: post
title: "Setting up Kubernetes Clusters"
author: "BoT"
tags: "devops"
excerpt_separator: <!--more-->
---

Setting up Kubernetes cluster locally is simple when all you need is `minikube`, but what if you have to do without?

<!--more-->

AKA how to setup Kubernetes in a production server.

## Prerequisite

Before we begin, let us first envision how our Kubernetes cluster would look like, assuming we have 5 servers of the following configuration:

- `master` with internal IP of `10.x.x.x`
- `worker-01` with internal IP of `10.y.y.1`
- `worker-02` with internal IP of `10.y.y.2`
- `worker-03` with internal IP of `10.y.y.3`
- `worker-04` with internal IP of `10.y.y.4`

Our goal here would be to establish the `master` server as the Kubernetes control plane and enroll all `worker` servers into the cluster. As there are multiple servers to keep track of, we will be splitting the commands required into three different categories:

1. **Kubernetes control plane**: commands executed in `master` server _only_
2. **Kubernetes workers**: commands executed in _all_ `worker` servers
3. **All participating nodes**: commands executed in _both_ `master` _and all_ `worker` servers

Note that all commands are to be executed as root user. And yes, we will be using the term "server" and "node" interchangeably because I can't make up my mind.

Anyway.

### At All Participating Nodes (as `master` and `worker-*`)

**Step 1. Update `hostname` and `/etc/hosts` of _each machine_ to allow easy identification**

```bash
# Edit existing hostname to desired name
hostnamectl set-hostname master

# Manually update /etc/hosts to add hostname
vim /etc/hosts
	127.0.0.1 localhost
	127.0.0.1 master
		[...truncated...]

# Confirm that changes has taken effect
cat /etc/hostname
	master
```

**Step 2. Install `kubeadm`, `kubectl` and `kubelet` tools into _each machine_**

```bash
sudo apt-get update
sudo apt-get install -y apt-transport-https ca-certificates curl

sudo curl -fsSLo /usr/share/keyrings/kubernetes-archive-keyring.gpg https://packages.cloud.google.com/apt/doc/apt-key.gpg

echo "deb [signed-by=/usr/share/keyrings/kubernetes-archive-keyring.gpg] https://apt.kubernetes.io/ kubernetes-xenial main" | sudo tee /etc/apt/sources.list.d/kubernetes.list

sudo apt-get update
sudo apt-get install -y kubelet kubeadm kubectl
sudo apt-mark hold kubelet kubeadm kubectl

# In some circumstances, docker service is not enabled
sudo systemctl enable docker
```

**Step 3. Reset any existing Kubernetes setup in _each machine_**

```bash
# Ensure environment is fresh for Kubernetes
kubeadm reset
rm -rf /etc/cni/net.d/

# Manually delete all containers within docker -- ONLY IF RESET COMMAND HANGS
docker rm -f $(docker ps -aq)

# Delete old network settings created by Flannel -- ONLY IF SERVER WAS PREVIOUSLY USING FLANNEL
ip link del cni0
ip link del flannel.1
systemctl restart NetworkManager
echo 1 > /proc/sys/net/ipv4/ip_forward
```

**Step 4. Add control-plane endpoint in to `/etc/hosts` at _each machine_**

```bash
# Manually update /etc/hosts to add Kubernetes control-plane endpoint
# Note that 10.x.x.x is the internal IP address of master node
vim /etc/hosts
	127.0.0.1		localhost
	127.0.0.1		master
		[...truncated...]
	10.x.x.x	control-plane

# ONLY IF NODE IS ENTERING THE CLUSTER WITH ITS PUBLIC IP
# 	Manually set the internal IP of node
# 	Note that 10.y.y.y is the internal IP address of a node
	10.y.y.y	worker
```

### At Kubernetes Control Plane (as `master`)

**Step 5. Initialize Master node as a Kubernetes control-plane**

Note that the values of the initialization command are decided based on several factors:

- `control-plane-endpoint`: User defined at `/etc/hosts` as per (4). Required to be defined in every node (both master and worker)
- `apiserver-advertise-address`: Refers to the internal IP address of the master node
- `pod-network-cidr`: No strict criteria, as long as the CIRR does not collide with anything else
- `service-cidr`: No strict criteria, as long as the CIRR does not collide with anything else

More often than not, `pod-network-cidr` and `service-cidr` can remain as `172.24.0.0/16` and `172.25.0.0/16` respectively since they are internal IP addresses and are isolated from other servers hosting services.

```bash
# Initialize a Kubernetes control-plane node
# Note that 10.x.x.x is the internal IP address of master node
kubeadm init --control-plane-endpoint=control-plane \
	--apiserver-advertise-address 10.x.x.x \
	--pod-network-cidr=172.24.0.0/16 \
	--service-cidr=172.25.0.0/16 \
	--upload-certs

# Apply Kubernetes configuration
echo "export KUBECONFIG=/etc/kubernetes/admin.conf" >> /root/.bashrc

# Disable swap -- ONLY IF SWAP ERROR OCCURS
# 	[ERROR Swap]: running with swap on is not supported.
swapoff -a

# Change Docker driver -- ONLY IF KUBELET COMPLAINS ABOUT CGROUP DRIVER MISCONFIGURATION
# 	Failed to run kubelet" err="failed to run Kubelet: misconfiguration:
# 	kubelet cgroup driver: \"systemd\" is different from docker cgroup driver: \"cgroupfs\"
vim /etc/docker/daemon.json
	{
	  "exec-opts": ["native.cgroupdriver=systemd"]
	}
sudo systemctl restart docker
kubeadm reset
kubeadm init ...	# Run init command again
```

**Step 6. Configure a layer 3 network fabric for Kubernetes cluster**

```bash
# Download Flannel installation script
wget https://raw.githubusercontent.com/coreos/flannel/master/Documentation/kube-flannel.yml

# Edit default IP range from 10.244.0.0/16 -> 172.24.0.0/16
# This is the pod-network-cidr previously configured
sed -i -- 's/10.244.0.0/172.24.0.0/g' kube-flannel.yml

# Update to grab any address within the advertised subnet of 10.y.y.y
vim ~/setup-flannel/kube-flannel.yml
   containers:
	   - name: kube-flannel
		   image: quay.io/coreos/flannel:v0.14.0
		   command:
		   - /opt/bin/flanneld
		   args:
		   - --ip-masq
		   - --kube-subnet-mgr
		   - --iface-regex=^10\. # initially was bond0

# Install Flannel
kubectl apply -f kube-flannel.yml
```

**Step 7. Update `iptables` to allow input from internal addresses**

```bash
# Assuming that all participating nodes are of 10.z.z.z as internal IP
iptables -A INPUT -s 10.0.0.0/8 -j ACCEPT
iptables -A INPUT -s 172.24.0.0/16 -j ACCEPT
iptables -A INPUT -s 172.25.0.0/16 -j ACCEPT
```

**Step 8. Generate token that allows other machines to join the cluster as workers**

```bash
# Command can be reran if the token has expired
kubeadm token create --print-join-command
```

### At Kubernetes Workers (as `worker-*`)

**Step 9. Reset any existing Kubernetes setup _within each worker nodes_ and join the master node**

```bash
# Reset any existing kubernetes setup within the machine
kubeadm reset
rm -rf /etc/cni/net.d/

# Command obtained from token creation command at master node
# Note that 10.y.y.y is the internal IP address of worker node
kubeadm join control-plane:6443 \
	--token bvczei.ser2qw9j5cuvuddq \
	--discovery-token-ca-cert-hash <TOKEN_ISSUED_BY_KUBEADM>
	--apiserver-advertise-address 10.y.y.y
```

**Step 10. Update `iptables` to ensure all nodes can communicate with each other**

```bash
# Assuming that all participating nodes are of 10.z.z.z as internal IP
iptables -A INPUT -s 10.0.0.0/8 -j ACCEPT
iptables -A INPUT -s 172.24.0.0/16 -j ACCEPT
iptables -A INPUT -s 172.25.0.0/16 -j ACCEPT
```

### At Kubernetes Control Plane (as `master`)

**Step 11. Check that everything was enrolled properly**

```bash
# Ensure that all pods (esp Flannel) are up and running
kubectl get pods -n kube-system -o wide

# Ensure that internal IPs are all set correctly
kubectl get pods -n argo -o wide
```

If the master node reports that all pods have the status of `Ready`, then congratulations! Have fun with your new cluster 👻
