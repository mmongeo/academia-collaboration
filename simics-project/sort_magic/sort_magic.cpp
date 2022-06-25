#include "quicksort.h"
#include "bubblesort.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include "magic-instruction.h"
#define QUICK_MAGIC 0 
#define BUBBLE_MAGIC 1 
#define STOP_PROFILING 2

using namespace std;
/*This code was developed by Mario Monge: mmongeo@gmail.com
 * */
void print_array(int * arr, int size) {
    for(int i = 0; i < size; ++i) {
        cout<< " " << arr[i];
    } 
    cout << "\n";
}


//Recieves filename with a space separated file with numbers to order in memory and the number of integers to sort
// sort_magic.o dataset.txt
int main (int argc, char **argv) {

    ifstream input (argv[1]);
    string line, num, temp;
    int size = 0;
    //traverse the file to see the length
    while (input >> temp){
        getline(input, line);
        stringstream str(line);
        while(getline(str, num, ',')) {
            ++size;
        }
    }
    //populate the arrays
    int cnt = 0;
    int * values_to_quick = new int[size + 1];
    int * values_to_bubble = new int[size + 1];
    input.clear();
    input.seekg(0);
    while (input >> temp){
        getline(input, line);
        stringstream str(line);
        while(getline(str, num, ',')) {
            values_to_quick[cnt] = stoi(num);
            values_to_bubble[cnt++] = stoi(num);
        }
    }
    Quicksort quick;
    MAGIC(QUICK_MAGIC);
    quick.quicksort(values_to_quick, 0, size - 1);
    MAGIC(STOP_PROFILING);
    Bubblesort bubble; 
    MAGIC(BUBBLE_MAGIC);
    bubble.bubblesort(values_to_bubble, size -1);
    MAGIC(STOP_PROFILING);
    //In case visualization is needed, recompile:
    //print_array(values_to_quick,size);
    //print_array(values_to_bubble, size);
    delete[] values_to_quick;
    delete[] values_to_bubble;
    return 0;
}
