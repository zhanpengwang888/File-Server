/sbin/iptables -A INPUT -p tcp --destination-port 8080 -j DROP
iptables -A OUTPUT -p tcp --destination-port 8080 -j DROP
                                                                
