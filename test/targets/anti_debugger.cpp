
#include <cstdio>
#include <unistd.h>
#include <signal.h>
#include <numeric>

void an_innocent_function() {
    std::puts("You just got bamboozled! You bimbo");
}

//used to work out the size of the code for an_innocent_function
void an_innocent_function_end() {}

//calculate checksum
;// also very dangerous way to do things, don't do this if you don't need to
int checksum() {
    //bytes of function should not change and also not be optimized by compilers
    auto start= reinterpret_cast<volatile const char *>(&an_innocent_function);
    auto end = reinterpret_cast<volatile const char *>(&an_innocent_function_end);

    //add up the bytes and return the sum
    return std::accumulate(start, end, 0);
}

int main(int argc, char ** argv) {
    auto safe = checksum();

    //write the address of an innocent function out into the world
    auto ptr = reinterpret_cast<void*>(&an_innocent_function); 
    write(STDOUT_FILENO, &ptr, sizeof(void*));
    fflush(stdout);

    raise(SIGTRAP);

    while (true){
        //check that checksum's not changed from the start
        if(checksum() == safe) {
            an_innocent_function();
        } else {
            //a breakpoint has been inserted
            std::puts("Ultra bamboozled bratan!");
        }

        fflush(stdout);
        raise(SIGTRAP);
    }

    return 0;
}

