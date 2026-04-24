#!/bin/bash

# Verificar si es root
if [[ "$EUID" -ne 0 ]]; then
    echo "Este script debe ejecutarse como root."
    echo "Usa: su -c './setup.sh'"
    exit 1
fi

echo "Instalando dependencias..."
apt-get update
apt-get install -y curl build-essential

# Preguntar al usuario si desea descargar ebooks
read -p "¿Desea descargar ebooks? (s/n): " respuesta

if [[ "$respuesta" == "s" || "$respuesta" == "S" ]]; then
    rm -rf gutenberg_books
    echo "Descargando ebooks..."
    ./scripts/download_gutenberg.sh
else
    echo "Se omitió la descarga de ebooks."
fi


# Compilar
echo "Compilando 'serial'..."
make -C serial || { echo "No se pudo compilar 'serial'"; exit 1; }

echo "Compilando 'parallel'..."
make -C parallel || { echo "No se pudo compilar 'parallel'"; exit 1; }

echo "Compilando 'concurrent'..."
make -C concurrent || { echo "No se pudo compilar 'concurrent'"; exit 1; }

echo "====== Compilación finalizada ======"

# Crear carpetas necesarias
mkdir -p comprimidos descomprimidos

# Menú interactivo
while true; do
    echo ""
    echo "Seleccione una opción:"
    echo "[1] Correr serial"
    echo "[2] Correr paralelo"
    echo "[3] Correr concurrente"
    echo "[4] Salir"
    read -p "Opción: " opcion

    clear

    case $opcion in
        1)
            echo "Ejecutando modo SERIAL..."
            ./serial/compress gutenberg_books comprimidos/serial.bin
            ./serial/decompress comprimidos/serial.bin descomprimidos/serial
            ;;
        2)
            echo "Ejecutando modo PARALELO..."
            ./parallel/compress gutenberg_books comprimidos/parallel.bin
            ./parallel/decompress comprimidos/parallel.bin descomprimidos/parallel
            ;;
        3)
            echo "Ejecutando modo CONCURRENT..."
            ./concurrent/compress gutenberg_books comprimidos/concurrent.bin
            ./concurrent/decompress comprimidos/concurrent.bin descomprimidos/concurrent
            ;;
        4)
            echo "Saliendo..."
            clear
            exit 0
            ;;
        *)
            echo "Opción inválida. Intente de nuevo."
            ;;
    esac
done
