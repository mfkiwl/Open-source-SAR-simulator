/*
 * Author: Alexander Rajula
 * Contact: alexander@rajula.org
 *
 * This simulator can either simulate and process simulated data,
 * or read pre-existing radar data from file, and then process it.
 *
 * The simulator utilizes standard SAR imaging and filtering algorithms, and the
 * processing chain typically looks something like this:
 * create an empty scene -> generate chirp waveform -> generate matched chirp waveform -> 
 * pulse compress single chirp waveform -> insert original (uncompressed) waveform in the middle of the scene ->
 * employ radar scanning algorithm -> employ RFI filtering (if needed) ->
 * employ pulse compression (if needed) -> employ GBP algorithm -> employ apodization (if needed) ->
 * calculate 2D FFT of GBP image -> write *all* data to file
 *
 * This file (sar_simulator.c) contains no algorithms, it just helps the user input data needed for simulation and
 * processing.
 */

#include "sar_simulator.h"
#include "cinsnowfilters.h"

int main(int argc, char** argv){
  radar_variables variables;
  matrix* data = malloc(sizeof(matrix)); 
  memset(data, 0, sizeof(matrix));
  strcpy(data->name, "metadata");

  printf("Do you wish to simulate or process radar data? (s/p): ");
  variables.mode = getchar();
  int ret;
  if(variables.mode == 'p'){
    printf("Please enter file name of raw data: ");
    ret = scanf("%s", variables.radar_data_filename);
    if(ret == EOF){
	printf("Invalid input detected, closing.\n");
	return 0;
    }
    if(!read_radar_file(data, &variables)){
	printf("Failed to read radar data, closing.\n");
	return 0;
    }
    process_data(data, &variables);
  }
  else if(variables.mode == 's'){
    /*
    printf("Simulate with real or complex values? (r/c): ");
    ret = scanf("%s", variables.real_or_complex_simulation);
    if(*variables.real_or_complex_simulation != 'r')
      *variables.real_or_complex_simulation = 'c';
    */

    ret = simulate(data, &variables);
    if(ret == -1)
      return 0;
    process_data(data, &variables);
  }
  else{
    printf("Mode not recognized - exiting.\n");
    return 0;
  }

  build_metadata(data, &variables);

  ret = write_data(data, &variables);
 
  //free_memory(data);

  return 0;
}

void free_memory(matrix* data_matrix){
  matrix* ptr = data_matrix;
  matrix* next = ptr->next;
  while(ptr != NULL){
    if(ptr->data != NULL)
      free(ptr->data);
    next = ptr->next;
    free(ptr);
    ptr = next;
  }
}

int simulate(matrix* data, radar_variables* variables){
  chirp_generator(data, variables);
  chirp_matched_generator(data, variables);
 
  matrix* meta_chirp = get_matrix(data, "chirp");
  complex double* chirp = meta_chirp->data;

  matrix* meta_match = get_matrix(data, "match");
  complex double* match = meta_match->data;

  matrix* meta_chirp_fft = get_last_node(data);
  meta_chirp_fft->data = malloc(meta_chirp->rows*sizeof(complex double));
  complex double* chirp_fft = meta_chirp_fft->data;
  meta_chirp_fft->rows = meta_chirp->rows;
  meta_chirp_fft->cols = meta_chirp->cols;
  strcpy(meta_chirp_fft->name, "chirp_fft");

  matrix* meta_match_fft = get_last_node(data);
  meta_match_fft->data = malloc(meta_match->rows*sizeof(complex double));
  complex double* match_fft = meta_match_fft->data;
  meta_match_fft->rows = meta_chirp->rows;
  meta_match_fft->cols = meta_chirp->cols;
  strcpy(meta_match_fft->name, "match_fft");

  fft_waveform(meta_chirp->rows, chirp, chirp_fft);
  fft_waveform(meta_match->rows, match, match_fft);

  pulse_compress_signal(data, variables);
  
  printf("Compressed pulse resolution: %lfm\n", calculate_compressed_pulse_resolution(data, variables));

  insert_waveform_in_scene(data,variables);

  radar_imager(data, variables);

  /*
  printf("Do you want to simulate radio-frequency interference (y/n)? ");
  do{
    ret = scanf("%c", &pc);
    if(pc == 'y')
      break;
    else if(pc == 'n')
      break;
  }while(1);
  if(pc == 'y'){
    printf("Adding GSM interference ... ");
    generate_gsm_interference(data, variables);
    printf("done.\n");
  }
  */

  return 0;
}

void process_data(matrix* data, radar_variables* variables){
  char pc = 0;
  int ret;

  printf("Do you want to employ CinSnow filtering to radar image (y/n)? ");
  do{
    ret = scanf("%c", &pc);
    if(pc == 'y')
      break;
    else if(pc == 'n')
      break;
  }while(1);
  if(pc == 'y'){
    printf("Running CinSnow filters ... ");
    cinsnowfilters(data, variables);
    printf("done.\n");
  }

  /*
  printf("Do you want to employ RFI suppression (y/n)? ");
  do{
    ret = scanf("%c", &pc);
    if(pc == 'y')
      break;
    else if(pc == 'n')
      break;
  }while(1);
  if(pc == 'y'){
    printf("Suppressing RFI ... ");
    rfi_suppression(data, variables);
    printf("done.\n");
  }
  else if(pc == 'n'){
    if(!data->unfiltered_radar_image){
	data->unfiltered_radar_image = malloc(variables->ncols*variables->nrows*sizeof(complex double));
	memcpy(data->unfiltered_radar_image, data->radar_image, variables->ncols*variables->nrows*sizeof(complex double));
    }
  }
  */
  
  printf("Do you want to enable pulse compression (y/n)? ");
  do{
    ret = scanf("%c", &pc);
    if(pc == 'y')
      break;
    else if(pc == 'n')
      break;
  }while(1);
  if(pc == 'y'){
    printf("Pulse-compressing image ... ");
    pulse_compress_image(data, variables);
    printf("done.\n");
  }
  
  gbp(data, variables);

  printf("Generating 2D FFT of GBP image ... ");
  gbp_fft(data, variables);
  printf("done.\n");
}

matrix* get_matrix(matrix* data, const char* name){
  if(data == NULL)
    return NULL;

  matrix* ptr = data;
  while(ptr != NULL){
    if( (strcmp(name, ptr->name) == 0) )
      return ptr;
    ptr = ptr->next;
  };
  return NULL;
}

matrix* get_last_node(matrix* data){
  if(data == NULL)
    return NULL;
  matrix* ptr = data;
  do{
    if(ptr->next == NULL)
      break;
    ptr = ptr->next;
  }while(1);

  ptr->next = malloc(sizeof(matrix));
  memset(ptr->next, 0, sizeof(matrix));
  return ptr->next;
}

void build_metadata(matrix* data, radar_variables* variables){
  matrix* ptr = data->next;
  unsigned int elements = 0;
  while(ptr != NULL){
    elements++;
    ptr = ptr->next;
  }
  data->data = malloc(elements*sizeof(complex double));
}
