make all - cria dois executáveis, ./client e ./server

make clean - limpa todos os objetos e executáveis


Criar duas placas de rede virtuais no computador:
```
ip netns add ns1
ip netns add ns2

ip link add veth1 type veth peer name veth2

ip link set veth1 netns ns1
ip link set veth2 netns ns2

ip netns exec ns1 ip link set dev veth1 address aa:bb:cc:dd:ee:01
ip netns exec ns1 ip addr add 10.0.0.1/24 dev veth1
ip netns exec ns1 ip link set dev veth1 up

ip netns exec ns2 ip link set dev veth2 address aa:bb:cc:dd:ee:02
ip netns exec ns2 ip addr add 10.0.0.2/24 dev veth2
ip netns exec ns2 ip link set dev veth2 up

ip netns exec ns2 ip link set dev lo up
ip netns exec ns1 ip link set dev lo up 
```

Executar o código na placa de rede virtual:
```
ip netns exec ns2 ./server
ip netns exec ns1 ./client
```
