# Instalacion Automatica


Nos descargamos el ZIP desde Github usando el comando `wget`
```bash
wget https://github.com/EstDan09/huffman-compression-c-serial-fork-pthreads/archive/refs/heads/main.zip
```

Lo movemos al directorio de Documentos y extraemos los archivos ahi

```bash
mv main.zip Documents
cd Documents
unzip main.zip
cd huffman-compression-c-serial-fork-pthreads
clear
```

Ejecutamos el instalador con los siguientes comandos:
```bash
chmod +x install.sh
su -c 'install.sh'
```