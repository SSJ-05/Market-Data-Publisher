# Market-Data-Publisher
Performance

Hardware:
- AMD Ryzen 7 7435HS
- Realtek RTL8168/8111
- 1 GbE

Configuration:
- DPDK + vfio-pci
- Single producer
- Single publisher
- Burst size 32
- Tick size 64 bytes
- Packet size 106 bytes

Results:
- Sustained lossless throughput: 500,000 PPS
- Measured duration: 15 minutes
- Packet loss: 0
