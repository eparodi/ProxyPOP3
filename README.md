# ProxyPOP3
## Materiales

* Archivo de construcción: `CMakeLists.txt`, ubicado en el directorio raíz.
* Informe: `docs/Informe.pdf`.
* Presentación: `docs/Presentación.pdf`.
* Códigos fuente: carpetas `pop3ctl`, `pop3filter` y `stripmime`.

## Compilación

Para compilar correr en el directorio raíz:
 
```
cmake .
make
```

### Artefactos generados

Los binarios se generan en la raíz del directorio con los nombres:

* pop3filter: server proxy.
* pop3ctl: cliente de configuración.
* stripmime: filtro de media types.

## Ejecución
### pop3filter
El proxy pop3 se ejecuta respetando las opciones y el argumento que sugiere el 
manual `pop3filter.8`.  
```
./pop3filter [options] <origin-server>
```
### stripmime
Se ejecuta corriendo. Utiliza las variables de entorno definidas por el manual
 `pop3filter.8`. 
```
./stripmime
```
### pop3ctl
El cliente de configuración se ejecuta corriendo:
```
./pop3ctl [options]
```
Ļas opciones disponibles son:
* -L \<management__address\> : Dirección del server de management
* -o \<management_port\> : Puerto del server de management