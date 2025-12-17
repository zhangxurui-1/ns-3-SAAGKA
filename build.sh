rm compile_commands.json
./ns3 configure
./ns3 build

cp cmake-cache/compile_commands.json ./
