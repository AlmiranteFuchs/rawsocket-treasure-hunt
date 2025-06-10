make - cria dois executáveis, ./client e ./server

make clean - limpa todos os objetos e executáveis


Criar duas placas de rede virtuais no computador:
In utils:
```
sudo loopback-setup.sh
```

Executar o código na placa de rede virtual:
```
ip netns exec ns2 ./server
ip netns exec ns1 ./client
```
