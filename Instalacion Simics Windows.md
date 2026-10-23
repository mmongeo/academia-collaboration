# Tutorial: Intel Simics Simulator Public Release

Consultar el siguiente enlace para obtener la mayor información posible: <https://www.intel.com/content/www/us/en/developer/articles/tool/simics-simulator.html>

## Guía de instalación

1. Instale la versión de Python 3.14.8 en su computadora
2. Descargue MINGW en su versión UCRT Win64 de la página [https://winlibs.com/#download-release](https://winlibs.com/#download-release)

   ![Página de descarga de MINGW en winlibs.com](images/image1.png)

3. Extraer y copiar el contenido extraído de MINGW en `C:\mingw64`
4. Asegurarse de que su carpeta de `C:\mingw64` contenga el directorio `bin` (`C:\mingw64\bin\`)
5. Navegar al enlace: <https://www.intel.com/content/www/us/en/developer/articles/tool/simics-simulator.html>
6. Hacer clic en el botón de `Download`:

   ![Botón “Download” en la página de Intel Simics](images/image2.png)

7. Esto los llevará a la siguiente página:

   ![Página de descargas de Intel Simics](images/image3.png)

8. Deben descargar ambos archivos.
9. Si no cuentan con una versión ya instalada (si usan Windows 11, este paso puede ser omitido), proceder al siguiente enlace para descargar el `Microsoft Visual C++ Redistributable for Microsoft Visual Studio 2015`: <https://www.microsoft.com/en-us/download/details.aspx?id=48145>
10. Instalar el paquete descargado de Microsoft
11. Una vez instalado (o si ya tenía una versión instalada), proceder a instalar el `intel-simics-package-manager-<version>-win64.exe`
12. Ejecutar el Intel Simics Package Manager
13. Seleccionar un directorio en donde se instalarán los paquetes de Simics. Se recomienda usar el directorio predeterminado: (`C:\Users\<NombreUsuario>\AppData\Local\Programs\Simics`) y solo darle el botón `Save`.
14. Una vez en esta ventana, darle al botón `Browse for Bundle`:

    ![Ventana del Intel Simics Package Manager con el botón “Browse for Bundle”](images/image4.png)

15. Buscar y seleccionar el paquete `simics-7-packages-<version>-win64.ispm` que descargaron anteriormente.
16. Presionar el botón `Create Project`.
17. Se desplegará una nueva ventana indicando todo lo que se va a instalar. Presionar `Proceed`
18. Si aparecen ventanas como la siguiente, solo deben darle `Yes`:

    ![Ventana de confirmación](images/image5.png)

19. Una vez finalizada la instalación, también se les habrá creado un proyecto de Simics en `C:\Users\NombreUsuario\simics-projects\my-intel-simics-project-1`

    Un proyecto de Simics es un directorio en donde desarrollarán sus modelos y podrán ejecutarlos con Simics. Pueden crear nuevos proyectos para diferentes modelos, pero no es necesario, ya que en un único proyecto pueden administrar varios modelos.

20. Confirmen que pueden navegar al directorio de su proyecto. Abran la terminal de Windows y ejecuten el comando `.\simics.bat`

    ![Terminal de Windows ejecutando Simics](images/image6.png)

    Con esta vista, ya están corriendo Simics.
