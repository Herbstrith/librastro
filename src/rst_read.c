/*
    Copyright (c) 1998--2006 Benhur Stein
    
    This file is part of Paj�.

    Paj� is free software; you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License as published by the
    Free Software Foundation; either version 2 of the License, or (at your
    option) any later version.

    Paj� is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
    for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with Paj�; if not, write to the Free Software Foundation, Inc.,
	51 Franklin Street, Fifth Floor, Boston, MA 02111 USA.
*/
#include "rst_private.h"

/*
  Internal functions
 */

// read an event from the buffer
static char *trd_event(rst_file_t *file, rst_event_t *event)
{
  //zero the event, prepare it for reading
  bzero (event, sizeof(rst_event_t));

  //the pointer to the data
  char *ptr = file->rst_buffer_ptr;

  //read the header
  u_int32_t header = RST_GET(ptr, u_int32_t);
  event->type = header >> 18;

  //if header contains time data with the hour, read it
  if (header & RST_TIME_SET) {
    //timestamp_t seconds = (timestamp_t) RST_GET(ptr, timestamp_t);
    //timestamp_t resolution = (timestamp_t) RST_GET(ptr, timestamp_t);
    //file->resolution = resolution;
    //file->hour = seconds * resolution;
  }

  //read the timestamp of this event, correct it according to the last hour
  //timestamp_t precision = (timestamp_t)RST_GET(ptr, u_int64_t);
//  timestamp_t time = file->hour + precision;
 // event->timestamp = (double)time/file->resolution;

  int fields_in_header = RST_FIELDS_IN_FIRST;
  char field_types[100];
  int i, field_counter = 0;
  for (;;) {
    for (i = 0; i < fields_in_header; i++) {
      int bits_to_shift = RST_BITS_PER_FIELD * (fields_in_header - i - 1);
      char type = (header >> bits_to_shift) & RST_FIELD_MASK;
      if (type == RST_NO_TYPE) {
        break;
      }
      field_types[field_counter++] = type;
    }
    if (!((header >> (fields_in_header * RST_BITS_PER_FIELD)) & RST_LAST)) {
      // there are more headers -- process next one
      header = RST_GET(ptr, u_int32_t);
      fields_in_header = RST_FIELDS_IN_OTHERS;
    } else {
      // it was last header -- stop
      break;
    }
  }
  for (i = 0; i < field_counter; i++) {
    switch (field_types[i]) {
    case RST_STRING_TYPE:
      RST_GET_STR(ptr, event->v_string[event->ct.n_string++]);
      break;
    case RST_DOUBLE_TYPE:
      event->v_double[event->ct.n_double++] = RST_GET(ptr, double);
      break;
    case RST_LONG_TYPE:
      event->v_uint64[event->ct.n_uint64++] = RST_GET(ptr, u_int64_t);
      break;
    case RST_FLOAT_TYPE:
      event->v_float[event->ct.n_float++] = RST_GET(ptr, float);
      break;
    case RST_INT_TYPE:
      event->v_uint32[event->ct.n_uint32++] = RST_GET(ptr, u_int32_t);
      break;
    case RST_SHORT_TYPE:
      event->v_uint16[event->ct.n_uint16++] = RST_GET(ptr, u_int16_t);
      break;
    case RST_CHAR_TYPE:
      event->v_uint8[event->ct.n_uint8++] = RST_GET(ptr, u_int8_t);
      break;
    }
  }

  //align the data pointer, return it for next read
  ptr = ALIGN_PTR(ptr);
  return ptr;
}

// correct a timestamp according to synchronization data 
static double rst_correct_time(rst_file_t *file, double remote)
{
  if (file->resolution == 0){
    fprintf(stderr,
            "[rastro_read] at %s, "
            "resolution written in the trace file is zero\n",
            __FUNCTION__);
    return 0;
  }
  double loc0 = file->sync_time.loc0/file->resolution;
  double ref0 = file->sync_time.ref0/file->resolution;
  return file->sync_time.a * (remote - loc0) + ref0;
}

// find synchronization data from a rastro_timesync file
static void find_timesync_data(char *filename, rst_file_t *file)
{
  char refhost[MAXHOSTNAMELEN];
  char host[MAXHOSTNAMELEN];
  timestamp_t time;
  timestamp_t reftime;

  //reset synchronization data for this file
  file->sync_time.a = 1;
  file->sync_time.loc0 = 0;
  file->sync_time.ref0 = 0;

  //open synchronization file for reading
  if (filename == NULL) {
    return;
  }
  FILE *ct_file = fopen(filename, "r");
  if (ct_file == NULL) {
    return;
  }

  //read all synchronization file, one line per time, parse it
  while (!feof(ct_file)) {
    if (fscanf(ct_file, "%s %lld %s %lld", refhost, &reftime, host, &time) != 4){
      break;
    }
    if (strcmp(refhost, file->hostname) == 0) {
      // this is the reference host
      file->sync_time.ref0 = reftime;
      file->sync_time.loc0 = reftime;
      break;
    }
    if (strcmp(host, file->hostname) == 0) {
      static int first = 1;
      if (first) {
        file->sync_time.ref0 = reftime;
        file->sync_time.loc0 = time;
        first = 0;
      } else {
        file->sync_time.a =
          (double) (reftime - file->sync_time.ref0) /
          (double) (time - file->sync_time.loc0);
      }
    }
  }
  fclose(ct_file);
}

void rst_fill_buffer(rst_file_t * file)
{
  int bytes_processed = file->rst_buffer_ptr - file->rst_buffer;
  int bytes_remaining = file->rst_buffer_used - bytes_processed;
  if (bytes_remaining >= RST_MAX_EVENT_SIZE) {
    return;
  }

  int bytes_free = file->rst_buffer_size - file->rst_buffer_used;
  if (bytes_free < RST_MAX_EVENT_SIZE) {
    memmove(file->rst_buffer, file->rst_buffer_ptr, bytes_remaining);
    file->rst_buffer_used = bytes_remaining;
    file->rst_buffer_ptr = file->rst_buffer;
    bytes_free += bytes_processed;
  }

  int bytes_read = read(file->fd, file->rst_buffer + file->rst_buffer_used, bytes_free);
  if (bytes_read > 0) {
    file->rst_buffer_used += bytes_read;
  }
}


/*
  Functions to read a single trace file
 */
static int rst_decode_one_event(rst_file_t *file, rst_event_t *event)

{
  //fill the file buffer by reading bytes from file
  rst_fill_buffer(file);

  //checks on bytes read
  int bytes_processed = file->rst_buffer_ptr - file->rst_buffer;
  int bytes_remaining = file->rst_buffer_used - bytes_processed;
  if (bytes_remaining <= 0) {
    return RST_NOK;
  }

  //read one event, update the buffer pointer
  file->rst_buffer_ptr = trd_event(file, event);

  //correct timestamp
  //event->timestamp = rst_correct_time(file, event->timestamp);

  //copy information from file to event
  event->id1 = file->id1;
  event->id2 = file->id2;
  event->file = file;

  //if event is the last one, stop
  if (event->type == RST_EVENT_STOP) {
    return RST_NOK;
  }

  //that's it, we have a new event
  return RST_OK;
}

//Open one trace file
static int rst_open_one_file(char *filename,
                             rst_file_t *file,
                             char *syncfilename, int buffer_size)
{
  static int ids = 0;

  //zero the file data structure
  bzero (file, sizeof(rst_file_t));
  file->id = ids++;

  //open file
  file->fd = open(filename, O_RDONLY);
  if (file->fd == -1) {
    fprintf(stderr, "[rastro] cannot open file %s: %s\n",
            filename, strerror(errno));
    return RST_NOK;
  }

  //reset clock synchronization data
  //since it is used to read the first event
  file->sync_time.a = 1.0;
  file->sync_time.ref0 = 0;
  file->sync_time.loc0 = 0;

  //the buffer_size must have space for two MAX events
  if (buffer_size < 2 * RST_MAX_EVENT_SIZE) {
    buffer_size = 2 * RST_MAX_EVENT_SIZE;
  }
  file->rst_buffer_size = buffer_size;

  //allocate memory space for buffer
  file->rst_buffer = (char *) malloc(file->rst_buffer_size);
  if (file->rst_buffer == NULL) {
    fprintf(stderr, "[rastro] cannot allocate buffer memory");
    close(file->fd);
    file->fd = -1;
    return RST_NOK;
  }
  file->rst_buffer_ptr = file->rst_buffer;
  file->rst_buffer_used = 0;

  //read the first event, must be a RST_EVENT_INIT with
  //- two u_int64_t for the ids
  //- the hostname where the trace was registered
  if (!rst_decode_one_event(file, &file->event)
      || file->event.type != RST_EVENT_INIT
      || file->event.ct.n_uint64 != 2
      || file->event.ct.n_string != 1) {
    fprintf(stderr, "[rastro] invalid rastro file %s\n", filename);
    close(file->fd);
    file->fd = -1;
    free(file->rst_buffer);
    file->rst_buffer = NULL;
    return RST_NOK;
  }

  //copy data from the first event to the file data structure
  file->id1 = file->event.id1 = file->event.v_uint64[0];
  file->id2 = file->event.id2 = file->event.v_uint64[1];
  file->hostname = strdup(file->event.v_string[0]);

  //save the filename of this trace file
  file->filename = strdup (filename);

  //find clock synchronization data, based on hostname of this trace
  find_timesync_data(syncfilename, file);

  //clock synchronization of the first event
  //file->event.timestamp = rst_correct_time(file, file->event.timestamp);
  return RST_OK;
}

static void rst_close_one_file(rst_file_t *file)
{
  //close and free
  close(file->fd);
  file->fd = -1;
  free(file->rst_buffer);
  file->rst_buffer = NULL;
  free(file->hostname);
  free(file->filename);
  file->hostname = NULL;
}


/* 
   Functions related to the reading of multiple trace files
 */
static void smallest_first(rst_rastro_t * f_data, int dead, int son)
{
  rst_file_t *aux;
  if (f_data->files[dead - 1]->event.timestamp >
      f_data->files[son - 1]->event.timestamp) {
    aux = f_data->files[dead - 1];
    f_data->files[dead - 1] = f_data->files[son - 1];
    f_data->files[son - 1] = aux;
  }
}

static void reorganize_bottom_up(rst_rastro_t * f_data, int son)
{
  int dead;
  dead = son / 2;
  if (dead == 0)
    return;

  smallest_first(f_data, dead, son);
  reorganize_bottom_up(f_data, dead);
}

static void reorganize_top_down(rst_rastro_t * f_data, int dead)
{
  int son1, son2;
  son1 = dead * 2;
  son2 = (dead * 2) + 1;

  //empty
  if (dead > f_data->n)
    return;
  //son1 does not belong
  if (son1 > f_data->n)
    return;
  //son2 does not belong
  if (son2 > f_data->n)
    smallest_first(f_data, dead, son1);

  //both belong
  else {
    if (f_data->files[son1 - 1]->event.timestamp <
        f_data->files[son2 - 1]->event.timestamp) {
      smallest_first(f_data, dead, son1);
      reorganize_top_down(f_data, son1);
    } else {
      smallest_first(f_data, dead, son2);
      reorganize_top_down(f_data, son2);
    }
  }
}

/*
  Public functions
 */
int rst_open_file(rst_rastro_t *rastro, int buffer_size, char *filename, char *syncfilename)
{
  if (rastro->files == NULL){
    rastro->files = (rst_file_t **) malloc(sizeof(*rastro->files));
    rastro->n = 0;

    rastro->emptyfiles = (rst_file_t **) malloc(sizeof(*rastro->files));
    rastro->size = 0;
  } else {
    rastro->files = (rst_file_t **) realloc(rastro->files,
                                            sizeof(*rastro->files) * (rastro->n + 1));
    rastro->emptyfiles = (rst_file_t **) realloc(rastro->emptyfiles,
                                       sizeof(*rastro->emptyfiles) * (rastro->n + 1));
  }

  if (rastro->files == NULL) {
    fprintf(stderr, "[rastro] cannot allocate memory");
    return RST_NOK;
  }

  rastro->files[rastro->n] =
      (rst_file_t *) malloc(sizeof(rst_file_t));
  bzero(rastro->files[rastro->n], sizeof(rst_file_t));
  if (rastro->files[rastro->n] == NULL) {
    fprintf(stderr, "[rastro] cannot allocate memory");
    return RST_NOK;
  }

  if (rst_open_one_file
      (filename, rastro->files[rastro->n], syncfilename,
       buffer_size)) {
    if (!rst_decode_one_event
        (rastro->files[rastro->n],
         &rastro->files[rastro->n]->event)) {

      //file is empty, save the pointer so we can free it later
      rastro->emptyfiles[rastro->size++] = rastro->files[rastro->n];
    } else {
      rastro->n++;
      reorganize_bottom_up(rastro, rastro->n);
    }
    return RST_OK;
  } else
    return RST_NOK;
}

void rst_close(rst_rastro_t *rastro)
{
  int i;

  //close and free active files
  for (i = 0; i < rastro->n; i++){
    rst_close_one_file (rastro->files[i]);
    free (rastro->files[i]);
  }
  free (rastro->files);
  rastro->n = 0;

  //close and free empty files
  for (i = 0; i < rastro->size; i++){
    rst_close_one_file (rastro->emptyfiles[i]);
    free (rastro->emptyfiles[i]);
  }
  free (rastro->emptyfiles);
  rastro->size = 0;
}

int rst_decode_event(rst_rastro_t *rastro, rst_event_t *event)
{
  rst_file_t *aux;

  //empty
  if (rastro->n < 1)
    return RST_NOK;

  else {
    *event = rastro->files[0]->event;

    rastro->n--;

    //switch the last and the first
    aux = rastro->files[0];
    rastro->files[0] = rastro->files[rastro->n];
    rastro->files[rastro->n] = aux;

    //reorganize
    reorganize_top_down(rastro, 1);

    if (!rst_decode_one_event
        (rastro->files[rastro->n],
         &rastro->files[rastro->n]->event)) {

      //file is empty, save the pointer so we can free it later
      rastro->emptyfiles[rastro->size++] = rastro->files[rastro->n];
    } else {
      rastro->n++;
      //reorganize
      reorganize_bottom_up(rastro, rastro->n);
    }
    return RST_OK;
  }
}


void rst_print_event(rst_event_t *event)
{
  int i;
  if (event->file->resolution > RST_MICROSECONDS){
    printf("%d type: %d ts: %.9f (id1=%"PRIu64",id2=%"PRIu64")\n", event->file->id,
           event->type, event->timestamp, event->id1, event->id2);
  }else{
    printf("%d type: %d ts: %f (id1=%"PRIu64",id2=%"PRIu64")\n", event->file->id,
           event->type, event->timestamp, event->id1, event->id2);
  }
  if (event->ct.n_uint64 > 0) {
    printf("\tu_int64_ts-> ");
    for (i = 0; i < event->ct.n_uint64; i++) {
      printf("(%" PRIu64 ") ", event->v_uint64[i]);
    }
    printf("\n");
  }
  if (event->ct.n_string > 0) {
    printf("\tstrings-> ");
    for (i = 0; i < event->ct.n_string; i++) {
      printf("(%s) ", event->v_string[i]);
    }
    printf("\n");
  }
  if (event->ct.n_float > 0) {
    printf("\tfloats-> ");
    for (i = 0; i < event->ct.n_float; i++) {
      printf("(%f) ", event->v_float[i]);
    }
    printf("\n");
  }
  if (event->ct.n_uint32 > 0) {
    printf("\tu_int32_ts-> ");
    for (i = 0; i < event->ct.n_uint32; i++) {
      printf("(%d) ", event->v_uint32[i]);
    }
    printf("\n");
  }
  if (event->ct.n_uint16 > 0) {
    printf("\tu_int16_ts-> ");
    for (i = 0; i < event->ct.n_uint16; i++) {
      printf("(%d) ", event->v_uint16[i]);
    }
    printf("\n");
  }
  if (event->ct.n_uint8 > 0) {
    printf("\tu_int8_ts-> ");
    for (i = 0; i < event->ct.n_uint8; i++) {
      printf("(%c) ", event->v_uint8[i]);
    }
    printf("\n");
  }
  if (event->ct.n_double > 0) {
    printf("\tdoubles-> ");
    for (i = 0; i < event->ct.n_double; i++) {
      printf("(%f) ", event->v_double[i]);
    }
    printf("\n");
  }
}
