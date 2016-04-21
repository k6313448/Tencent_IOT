int get_interval(x.value, char char_num){
    for(int i=0; i<10; i++){
        if(x.value[i]==':'){
            char_num=x.value[i+1];
            return 0;
        }
    }
    return 1;
}
