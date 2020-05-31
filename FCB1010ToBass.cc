#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <alsa/asoundlib.h>
#include <alloca.h>

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
  while (1) {
    snd_seq_event_t *ev;
  
    while(snd_seq_event_input(seq_handle, &ev) >= 0) {
      if (ev->type == SND_SEQ_EVENT_CONTROLLER) {
        snd_seq_ev_ctrl_t& ctrl = ev->data.control;
        printf("Got note-on/off: channel %d param %d value %d\n", ctrl.channel, ctrl.param, ctrl.value);fflush(stdout);
        switch (ctrl.param) {
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
        note.note = ctrl.value + 59;
        note.velocity = 120;
      }
      snd_seq_ev_set_source(ev, midi_device_port.port);
      snd_seq_ev_set_subs(ev);  
      snd_seq_ev_set_direct(ev);
      snd_seq_event_output_direct(seq_handle, ev);
      snd_seq_drain_output(seq_handle);
      //snd_seq_free_event(ev);
    }
  }
}


