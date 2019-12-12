#!/usr/bin/env python3

import subprocess

n = 3
base_port = 5000
peers_ip = ['127.0.0.1', '127.0.0.1', '127.0.0.1']

sub_process = []

for me in range(n):
    kvserver_location = '../build/kvraft/KvServer'
    cmd = '%s %s %s %s '%(kvserver_location, n, me, base_port) + " ".join(peers_ip)
    p = subprocess.Popen(cmd, shell=True)
    sub_process.append(p)
    print("start kvserver with " + cmd)

for p in sub_process:
    p.wait()
    
