#include <random>
#include <iostream>
int main(){
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0,255);
    for (int i=0;i<5;++i){ int v=dist(rng); std::cout<<v<<" "; }
    std::cout<<"\n";
    return 0;
}
