#!/bin/bash

# filepath: /home/jdiaz/Documents/SO/P1-Git/Proyecto1Opera/P1/install_dependencies.sh

# Obtener el directorio actual del script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "Instalando dependencias..."
sudo apt update
sudo apt install -y build-essential

echo "Compilando proyecto..."
cd "$SCRIPT_DIR"
make clean
make all
echo "Compilación completa"
ls -la huffman_*