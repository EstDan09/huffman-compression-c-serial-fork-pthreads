# Manual de Uso

## Prerequisitos:

- Tener instalado Debian 13
- Tener instalado herramientas como curl y build-essentials

En caso de no tener instalados esas herramientas, ejecutar los siguientes comandos

Abrir la terminal e ingresar como super usuario 
```bash
su
# Ingrese su contraseña.
```

```bash
# Instalar curl
apt-get install curl
# ingresamos Y

# instalar build-essential
apt-get install build-essential
# ingresamos Y
```

Y nos salimos del modo super-usuario
```bash
exit
```

## Instalación

Una vez instalado las herramientas, procedemos a descargar el archivo ZIP que se encuentra en el repositorio de Github utilizando el comando `wget`
```bash
wget https://github.com/EstDan09/huffman-compression-c-serial-fork-pthreads/archive/refs/heads/main.zip
```

Una vez descargado verificamos si se encuentra en el directorio con el comando `ls`, en caso que deseamos moverlo a un directorio nuevo como `Downloads` o `Documents` podemos usar el comando `mv`

```bash
# ejemplo moviendo el ZIP a carpeta Documents
ls
mv main.zip Documents
cd Documents
```

Dentro del directorio en el que tenemos descargado el ZIP, vamos a extraer todos los archivos utilizando el comando `unzip` y usando el comando `ls` verificamos si todo se encuentra.

```bash
unzip main.zip
ls
```

Realizado la extracción podemos ahora ejecutar el proyecto, como primer paso nos dirigimos dentro de la carpeta extraída
```bash
cd huffman-compression-c-serial-fork-pthreads-main

# verificamos que existan archivos
ls
```

### 1. Descarga de Top 100 Libros del Proyecto Gutenberg
Este paso se puede omitir si desea utilizar los libros existentes previamente descargados del repositorio, si se omite, pasar al paso 2. 

En caso que no se desee, procedemos a eliminar la carpeta
```bash
rm -rf gutenberg_books

# verificamos que no se encuentre en el directorio
ls
```

Como primer paso, vamos a descargar el TOP 100 de libros en el proyecto Gutenberg, para esto nos dirigimos a la carpeta `scripts` y ejecutamos el script de `download_gutenberg.sh`

```bash
cd scripts
./download_gutenberg.sh
cd ..

# verificamos que se encuentre la carpeta gutenberg_books
ls

# verificamos que se encuentren libros
ls gutenberg_books

# verificamos la cantidad de libros descargados (debe indicar mas de 100)
cd gutenberg_books
ls | wc -l
cd ..
```

**Nota:** en caso que se caiga la ejecución o no logre llegar a los 100, volver a ejecutar el script. Si no, instalar uno por uno, pues pasa que a veces existen audio libros.

## Serial
### 1. Comprimir en forma Serial
Una vez verificado que se encuentan los libros en la carpeta `gutenberg_books` procedemos a comprimir los libros utilizando el algoritmo de Huffman de forma Serial, nos dirigimos a la carpeta `serial`

```bash
cd serial
```

Primero, debemos compilar los ejecutables, para esto tenemos nuestro `Makefile`

```bash
make
# ignorar los warnings

clear
ls
```

Una vez compilado, procedemos a comprimir los libros descargados en un archivo binario

```
./compress ../gutenberg_books comprimido_serial.bin
```

Una vez comprimido los libros, podemos ver cuanto pesa en comparación a los descargados usando el comando `du`

```bash
# ver el peso del comprimido
du -h comprimido_serial.bin

# ver el peso de gutenberg_books
du -h ../gutenberg_books
```

### 2. Descomprimir en forma Serial
Para descomprimir utilizaremos el ejecutable `decompress`, le ingresamos un archivo comprimido y en cual directorio se va a guardar dicha salida.

```bash
./decompress comprimido_serial.bin descomprimido_serial
```

Una vez completado, podemos verificar que tanto el directorio descomprido como el de gutenberg_books pesan lo mismo

```bash
du -h descomprimido_serial
du -h ../gutenberg_books
```

Podemos ver que el tiempo de descomprido dura un poco mas que el doble de comprimir.

## Paralelo

Una vez probado el modo serial, ahora vamos a ver como funciona con el metodo paralelo. 

Para esto, vamos a regresarnos al directorio del repositorio descargado y entramos a la carpeta de `parallel`

```bash
cd ..
cd parallel

# limpiamos
clear

ls
```

Una vez dentro de la carpeta parallel, vamos a compilar los ejecutables usando el comando `make`

```bash
make
# ignorar los errores

clear

# revisamos que esten los ejecutables compress y decompress
ls
```

### Comprimir en Paralelo
Una vez compilado los ejecutables procedemos a correr el ejecutable `compress`, para comprimir en paralelo todos los libros del proyecto Gutenberg, dicho resultado sera guardado en el archivo `comprimido_parallel.bin`.

```bash
./compress ../gutenberg_books comprimido_parallel.bin
```

Los resultados de ejecución en paralelo en modo de pruebas ha sido 3/7 parte de lo que tarda en serial, es decir, un +40% más rápido que en serial.

Si queremos ver el tamaño del archivo generado, es igual que en serial
```bash
du -h comprimido_parallel.bin
```

### Descomprimir en Paralelo
Similar a lo en la parte serial, para descomprimir el comprimido lo que debemos hacer es utilizar el ejecutable `decompress`, vamos a poner de entrada el archivo comprimido `comprimido_parallel.bin` y el resultado será guardado en `descomprimido_parallel`

```bash
./decompress comprimido_parallel.bin descomprimido_parallel
```
El proceso de descomprimido en paralelo es una cuarta parte más rápido de lo que duró en serial. 

Si vemos el tamaño del resultado con `du -h descomprimido_parallel`, es el mismo tamaño que `gutenberg_books`.

## Concurrente

Una vez probado concurrente, ahora nos regresamos al directorio principal para entrar al directorio de `concurrent`

```bash
cd ..
cd concurrent

clear
ls
```

Compilamos los ejecutables con `make`
```bash
make
# ignorar los warnings

clear

# verificar
ls
```

### Comprimir Concurrente
Lo mismo que lo anterior, vamos a comprimir los libros de `gutenberg_books`, y lo vamos a guardar en `comprimido_concurrent.bin`

```bash
./compress ../gutenberg_books comprimido_concurrent.bin
```
De esta prueba, podemos ver que es ligeramente más rápido que procesarlo en paralelo. 

Siendo así, la ejecución en concurrente donde mejor tiempo tiene para comprimir.

### Descomprimir Concurrente
Se va a descomprimir en un directorio llamado `descomprimido_concurrente`.

```bash
./decompress comprimido_concurrent.bin descomprimido_concurrente
```

Donde también, tiene un procesado un poco más rápido que la forma paralela.

Podemos verificar los tamaños de los archivos y directorios para confirmar que dicho comprimido y descomprimido son iguales en todos

```bash
du -h comprimido_concurrent.bin
du -h descomprimido_concurrente
du -h ../gutenberg_books
```












