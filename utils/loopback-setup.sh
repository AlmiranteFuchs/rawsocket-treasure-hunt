if [ "$EUID" -ne 0 ]; then
  echo "Please run as root (use sudo)."
  exit 1
fi

echo "Starting virtual network setup..."
echo "----------------------------------------"

# 1. Deletar namespaces antigos, caso existam, para evitar erros
echo "[Step 1] Cleaning up old configurations (if they exist)..."
ip netns del ns1 &> /dev/null
ip netns del ns2 &> /dev/null

# 2. Criar dois namespaces de rede
echo "[Step 2] Creating network namespaces: ns1 and ns2..."
ip netns add ns1
ip netns add ns2

# 3. Criar um par de interfaces veth (um "cabo de rede virtual")
echo "[Step 3] Creating veth pair (veth1 <--> veth2)..."
ip link add veth1 type veth peer name veth2

# 4. Mover cada ponta do "cabo" para um namespace
echo "[Step 4] Moving interfaces to their respective namespaces..."
ip link set veth1 netns ns1
ip link set veth2 netns ns2

# 5. Configurar a interface dentro do namespace ns1
echo "[Step 5] Configuring interface veth1 in namespace ns1..."
ip netns exec ns1 ip link set dev veth1 address aa:bb:cc:dd:ee:01
ip netns exec ns1 ip addr add 10.0.0.1/24 dev veth1
ip netns exec ns1 ip link set dev veth1 up
ip netns exec ns1 ip link set dev lo up # Ativar também a interface de loopback

# 6. Configurar a interface dentro do namespace ns2
echo "[Step 6] Configuring interface veth2 in namespace ns2..."
ip netns exec ns2 ip link set dev veth2 address aa:bb:cc:dd:ee:02
ip netns exec ns2 ip addr add 10.0.0.2/24 dev veth2
ip netns exec ns2 ip link set dev veth2 up
ip netns exec ns2 ip link set dev lo up # Ativar também a interface de loopback

echo "----------------------------------------"
echo "Setup completed successfully!"
echo ""