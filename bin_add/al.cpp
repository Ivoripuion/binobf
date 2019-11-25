#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#define  NOT_EXIST -1;
typedef struct{
	char** seq;
	int*   code;
	int    size;
	int    max_size;
}Dicionary;

void insert_seq(Dicionary*dict,char*seq){
	int i = dict->size;
	dict->seq[i] = (char*)(malloc(sizeof(char)*strlen(seq)+1));
	dict->code[i] = i;
	dict->size++;
	strcpy(dict->seq[i], seq);

}

void init_dictionary(Dicionary* dict,int max_size){
	dict->max_size = max_size;
	dict->size     = 0;
	dict->seq      = (char**)(malloc(sizeof(char*)* max_size));
	dict->code     = (int*)(malloc(sizeof(int)*max_size));

	insert_seq(dict,"#");
	char seq[2] = "A";
	for (int i = 0; i < 26; i++){
		insert_seq(dict,seq);
		seq[0]++;
	}
}

int get_seq_code(Dicionary*dict,char*seq){
	for (int i = 0; i < dict->size; i++){
		if (strcmp(dict->seq[i], seq) == 0){
			return dict->code[i];
		}
	}
	return NOT_EXIST;
}

char* get_code_seq(Dicionary*dict,int code){
	if (code<0||code>dict->size){
		return NULL;
	}
	else{
		int i = code;
		return dict->seq[i];
	}
}

void lzw_encode(char*data,Dicionary*dict,int *output){//—πÀı
	char current[1000];
	char next;
	int code;
	int i=0;
	int output_index = 0;
	while (i<strlen(data)){
		sprintf(current,"%c",data[i]);
		next = data[i+1]; 
		while ( get_seq_code(dict,current) != -1 ){
			sprintf(current,"%s%c",current,next);
			i++;
			next = data[i+1];
		}
		current[strlen(current) - 1] = '\0';
		next = data[i];
		code = get_seq_code(dict,current);
		sprintf(current, "%s%c", current, next);
		insert_seq(dict,current);
		output[output_index] = code;
		output_index++;
		//printf("%d,",code);
	}
	output[output_index] = '\0';
	for (int i = 0; output[i] != '\0'; i++){
		printf("%d,",output[i]);
	}
}

void lzw_decode(int codes[],int n,Dicionary*dict){//Ω‚—π
	int code;
	char prev[1000];
	char*output;

	code = codes[0];
	output = get_code_seq(dict,code);
	printf("%s",output);

	int i;
	for (i = 1; i < n;i++){
		code = codes[i];
		strcpy(prev,output);
		output = get_code_seq(dict,code);
		sprintf(prev,"%s%c",prev,output[0]);
		insert_seq(dict,prev);

		printf("%s",output);
	}
}

void print_dict(Dicionary*dict){//¥Ú”°◊÷µ‰
	printf("===============\n");
	printf("Code   Sequence\n");
	printf("===============\n");
	for (int i = 0; i < dict->size; i++){
		printf("%5d%4c",dict->code[i],'   ');
		printf("%s\n",dict->seq[i]);
	}
	printf("===============\n");
}

int CompressAndEncrypt(char* inbuf, int* outbuf)
{
	Dicionary dict;
	init_dictionary(&dict, 10000);
	print_dict(&dict);
	// 1. —πÀı
	lzw_encode(inbuf,&dict,outbuf);
	// 2. º”√‹
	return 1;
}

int main(){
	//Dicionary dict;
	//init_dictionary(&dict,10000);
	//print_dict(&dict);
	//lzw_encode("HOWABOUTTHATTAHT",&dict);
	//int arr[15] = { 8, 15, 23, 1, 2, 15, 21, 20, 20, 8, 1, 34, 1, 8, 20 };
	//lzw_decode(arr,15,&dict);
	char buffer[] = "AKDBQIQODMOQ";
	int output[1000];
	CompressAndEncrypt(buffer,output);
	return 0;
}