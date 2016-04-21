#include <stdio.h>
#include <stdlib.h>
char get_interval(char s[20])
{
    int i=0;
    for(i=0;i<20;i++)
        if(s[i]==':'){
            return (s[i+1]);
        }
    return 0;
}
int main(){
    int a=0;
    char input[20]="asf:43dd";
    a=get_interval(input);
    printf("%c\n",a);
    return 0;
}
