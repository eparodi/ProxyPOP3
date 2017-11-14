# ProxyPOP3
## Materiales

* Archivo de construcción: `CMakeLists.txt`, ubicado en el directorio raíz.
* Informe: `docs/Informe.pdf`.
* Presentación: `docs/Presentación.pdf`.
* Códigos fuente: carpetas `POP3ctl`, `POP3filter` y `stripMIME`.

## Compilación

Para compilar correr en el directorio raíz:
 
```
cmake .
make
```

### Artefactos generados

Se generan tres binarios en la raíz del directorio con los nombres:

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
Utiliza las variables de entorno definidas por el manual `pop3filter.8`.
Se ejecuta corriendo: 
```
./stripmime
```
### pop3ctl
El cliente de configuración se ejecuta corriendo:
```
./pop3ctl [options]
```
Las opciones disponibles son:

* -L \<management_address\> : dirección del server de management
* -o \<management_port\> : puerto del server de management