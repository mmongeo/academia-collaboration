#ifndef _MEHQUICKSO
#define _MEHQUICKSO
#include <cstdlib>
/* This code was developed by Mario Monge 2010
 * */
class Quicksort{
private:
unsigned int partition(int * arreglo, unsigned int inicio, unsigned int fin){
	unsigned int i = inicio;
	unsigned int j = fin;
	
	unsigned int randomPos = rand() % (fin - inicio);
	int auxiliar = arreglo[inicio + randomPos];
	arreglo[inicio + randomPos] = arreglo[fin];
	arreglo[fin] = auxiliar;
	int pivot = arreglo[fin];
	
	while(i != j){
		if(arreglo[i]<=pivot){
			++i;
		}else{
			--j;
			if(arreglo[j]<pivot){ // inutuk
				auxiliar = arreglo[j];
				arreglo[j] = arreglo[i];
				arreglo[i] = auxiliar;
				++i;
			}
		}
	}
	if(j!=fin){
		arreglo[fin] = arreglo[i];
		arreglo[j] = pivot;
	}
	return i;
}

public:
/*
Utiliza partition para ordenar un arreglo del principio del inicio al final de este.
El final no es el tamano del arreglo sino el indice final del arreglo. 
Va partiendo el arreglo en partes dependientes del pivot, llamandose recursivamente.
*/
void quicksort(int * arreglo, unsigned inicio, unsigned fin){
	if(inicio<fin){
		unsigned int posAcomodada = partition(arreglo,inicio,fin);
		if(posAcomodada != inicio){ 
			quicksort(arreglo,inicio,posAcomodada-1);
		}
		if(posAcomodada != fin){
			quicksort(arreglo, posAcomodada + 1, fin);
		}
	}
}
};
#endif
