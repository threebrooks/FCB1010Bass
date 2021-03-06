#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <alsa/asoundlib.h>
#include <alloca.h>
#include <vector>

int param_val_to_note(int val, int octave) {
  int delta = 0;
  if (val == 1) delta = 0; // 1
  else if (val == 6) delta = 1; // 6
  else if (val == 2) delta = 2; // 2
  else if (val == 7) delta = 3; // 7
  else if (val == 3) delta = 4; // 3
  else if (val == 8) delta = 5; // 8 
  else if (val == 4) delta = 6; // 4
  else if (val == 9) delta = 7; // 9
  else if (val == 5) delta = 8; // 5
  else if (val == 0) delta = 9; // 10
  else if (val == 11) delta = 10; // Down
  else if (val == 10) delta = 11; // Up
  return 40+12*octave+delta;
}

int get_octave(int val) { 
  if (val >= (0.9*128)) return 1;
  else if (val <= 0.1*128) return -1;
  else return 0;
}

std::vector<int> sequence = {1,0,-1,0,1,0,-1};
int idx_in_sequence = 0;
void add_shutdown_event(int val) {
  int new_pos = sequence[idx_in_sequence];
  if (val >= (0.9*128)) new_pos = 1;
  else if (val >= (0.3*128) && val <= (0.7*128)) new_pos = 0;
  else if (val <= 0.1*128) new_pos = -1;
  //printf("val %i newpos %i\n",val,new_pos);
  if (sequence[idx_in_sequence] == new_pos) {
  } else if (sequence[idx_in_sequence+1] == new_pos) {
    idx_in_sequence++; 
    //printf("Advance\n");
  } else {
    idx_in_sequence = 0;
    //printf("Reset\n");
  }
  if (idx_in_sequence >= sequence.size()-1) {
    printf("Shutdown\n");
    system("shutdown -h now");
  }
}

int main(int argc, char *argv[]) {

  snd_seq_t *seq_handle;
  int npfd;
  char* midi_device_port_string;
  struct pollfd *pfd;

  if (argc < 2) {
    fprintf(stderr, "midi-pitch-to-controller <midi device port, aseqdump -l>\n");
    exit(1);
  } else {
    midi_device_port_string = argv[1];  
  }  

  if (snd_seq_open(&seq_handle, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
    fprintf(stderr, "Error opening ALSA sequencer.\n");
    return -1;
  }
  snd_seq_set_client_name(seq_handle, "translator");

  int in_port = snd_seq_create_simple_port(seq_handle, "pitch-to-controller in", SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE|SND_SEQ_PORT_TYPE_MIDI_GENERIC, SND_SEQ_PORT_TYPE_APPLICATION);
  if (in_port < 0) {
    fprintf(stderr, "Error creating sequencer port.\n");
    return -1;
  } 

  snd_seq_addr_t midi_device_port;
  snd_seq_parse_address(seq_handle, &midi_device_port, midi_device_port_string);
  printf("client: %d port: %d\n",midi_device_port.client, midi_device_port.port);
  int err;
  if ((err = snd_seq_connect_to(seq_handle, 0, midi_device_port.client, midi_device_port.port)) < 0) {
    fprintf(stderr,"Cannot connect to port %d:%d %s", midi_device_port.client, midi_device_port.port, snd_strerror(err));
    return -1;
  }  

  if ((err = snd_seq_connect_from(seq_handle, 0, midi_device_port.client, midi_device_port.port)) < 0) {
    fprintf(stderr,"Cannot connect from port %d:%d %s", midi_device_port.client, midi_device_port.port, snd_strerror(err));
    return -1;
  }

  int octave = 0;

  snd_seq_event_t all_off_ev;
  snd_seq_ev_set_source(&all_off_ev, midi_device_port.port);
  snd_seq_ev_set_subs(&all_off_ev);  
  snd_seq_ev_set_direct(&all_off_ev);
  all_off_ev.type = SND_SEQ_EVENT_CONTROLLER;
  while (1) {
    snd_seq_event_t *ev;
  
    while(snd_seq_event_input(seq_handle, &ev) >= 0) {
      if (ev->type == SND_SEQ_EVENT_CONTROLLER) {
        snd_seq_ev_ctrl_t& ctrl = ev->data.control;

        all_off_ev.data.control.channel = ctrl.channel;  
        all_off_ev.data.control.param = MIDI_CTL_ALL_SOUNDS_OFF;  
        snd_seq_event_output_direct(seq_handle, &all_off_ev);
        all_off_ev.data.control.param = MIDI_CTL_RESET_CONTROLLERS;  
        snd_seq_event_output_direct(seq_handle, &all_off_ev);
        snd_seq_drain_output(seq_handle);
       
        //printf("Got note-on/off: channel %d param %d value %d\n", ctrl.channel, ctrl.param, ctrl.value);fflush(stdout);
        switch (ctrl.param) {
          case 102:
          {
            octave = get_octave(ctrl.value);
          }
          break;
          case 103:
          {
            add_shutdown_event(ctrl.value);
          }
          break;
          case 104:
          {
            ev->type = SND_SEQ_EVENT_NOTEON;
          }
          break;
          case 105:
          {
            ev->type = SND_SEQ_EVENT_NOTEOFF;
          }
          break;
        }
        snd_seq_ev_note_t& note = ev->data.note;
        note.channel = ctrl.channel;
        note.note = param_val_to_note(ctrl.value, octave);
        note.velocity = 120;
      }
      snd_seq_ev_set_source(ev, midi_device_port.port);
      snd_seq_ev_set_subs(ev);  
      snd_seq_ev_set_direct(ev);
      snd_seq_event_output_direct(seq_handle, ev);
      snd_seq_drain_output(seq_handle);
    }
  }
}


