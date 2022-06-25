#ifndef _MEHBUBBLE
#define _MEHBUBBLE
/* This code was developed by Mario Monge 2010
 * */
class Bubblesort{
private:
    void cambio(int * arreglo, int pos, int otra_pos){
        int cambio; 
        cambio = arreglo[pos];
        arreglo[pos] = arreglo[otra_pos];
        arreglo[otra_pos] = cambio;
    } 

public:
/*
Utiliza partition para ordenar un arreglo del principio del inicio al final de este.
El final no es el tamano del arreglo sino el indice final del arreglo. 
Va partiendo el arreglo en partes dependientes del pivot, llamandose recursivamente.
*/
void bubblesort(int * arreglo, unsigned fin){
    for(int i = 0; i < fin; ++i){
        for(int j = 0; j < fin - i; ++j){
            if(arreglo[j] > arreglo[j + 1])
                cambio(arreglo, j, j +1);
        }
    }
}
};
#endif
