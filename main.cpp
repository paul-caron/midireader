/*

Title: Midi File Reader

Author: Paul Caron

Version: 0.0.0

Date: February 9th 2024

Description: Parses a MIDI file and reads its content

*/

#include <iomanip>
#include <iostream>
#include <fstream>
#include <stack>
#include <string>
#include <vector>
#include <map>

std::map<unsigned char, std::string> metaEventTypes{
{0x00, "Sequence Number"},
{0x01, "Text Event"},
{0x02, "Copyright Notice"},
{0x03, "Sequence/Track Name"},
{0x04, "Instrument Name"},
{0x05, "Lyric"},
{0x06, "Marker"},
{0x07, "Cue Point"},
{0x20, "MIDI Channel Prefix"},
{0x2f, "End of Track"},
{0x51, "Set Tempo"},
{0x54, "SMPTE Offset"},
{0x58, "Time Signature"},
{0x59, "Key Signature"},
{0x7f, "Sequencer Specific Meta-Event"},
};

unsigned int as32uint(unsigned char arr[4]){
  const unsigned int n = arr[3] +
                         (arr[2] << 8) +
                         (arr[1] << 16) +
                         (arr[0] << 24);
  return n ;
}

unsigned short int as16uint(unsigned char arr[2]){
  const unsigned short int n = arr[1] + (arr[0] << 8);
  return n ;
}

unsigned int unstackVariableNumber(std::stack<unsigned char> & stk){
  unsigned int result{0};
  unsigned int level{0};
  while(!stk.empty()){
    unsigned int top = stk.top() & 0b01111111;
    stk.pop();
    unsigned shift = level * 7;
    result += top << shift;
    level += 1;
  }
  return result;
}

class MidiReader{

private:
  std::ifstream midiFile;
  unsigned char one;
  unsigned char two[2];
  unsigned char four[4];

  unsigned char lastOp{};
  unsigned char lastChannel{};

  std::string headerChunkType{};
  unsigned int headerLength{0};
  unsigned short int fileFormat{0};
  unsigned short int numberOfTracks{0};
  unsigned short int divisionTime{0};

  std::vector<unsigned long long> trackPointers;

public:
  MidiReader(const char * filename){
    std::cout << "Reading file " << filename << "." << std::endl;
    midiFile.open(filename, std::ios::binary) ;

    //read header data
    std:: cout << std::string(30, '_') << std::endl;
    midiFile.read(reinterpret_cast<char *>(four), 4);
    headerChunkType = std::string(four, four+4) ;
    midiFile.read(reinterpret_cast<char *>(four), 4);
    headerLength = as32uint(four);
    midiFile.read(reinterpret_cast<char *>(two), 2);
    fileFormat = as16uint(two);
    midiFile.read(reinterpret_cast<char *>(two), 2);
    numberOfTracks = as16uint(two);

    midiFile.read(reinterpret_cast<char *>(two), 2);
    divisionTime = as16uint(two);

    printHeaderContent();

    //find the track positions in the file stream
    midiFile.seekg(std::ios_base::beg);
    for(unsigned short n=0;n<numberOfTracks;n++){
      unsigned long long int tp = getNextTrackPointer();
      trackPointers.push_back(tp);
      midiFile.seekg(tp);
    }
  }
  void printHeaderContent(){
    std::cout << "Chunk Type: " << headerChunkType << std::endl;
    std::cout << "Chunk Length: " << headerLength << std::endl;
    std::cout << "File Format: " << fileFormat << std::endl;
    std::cout << "Number of Tracks: " << numberOfTracks << std::endl;
    if(divisionTime & 0x8000){
      std::cout << "Division Time Type: SMPTE" << std::endl;
      char timeFormat = (divisionTime & 0xff00) >> 8 ;
      unsigned char ticksPerFrame = divisionTime & 0x00FF;
      std::cout << "Division Time Format:" << +timeFormat << std::endl;
      std::cout << "Ticks Per Frame: " << +ticksPerFrame << std::endl;
    }else{
      std::cout << "Division Time Type: PPQ" << std::endl;
      std::cout << "Division Time Ticks Per Quarter Note: " << as16uint(two) << std::endl;
    }
  }
  unsigned long long int getNextTrackPointer(){
    midiFile.read(reinterpret_cast<char *>(four), 4);
    std::string trackChunkType(four, four+4);

    midiFile.read(reinterpret_cast<char *>(four), 4);
    unsigned long long int trackLength = as32uint(four);
    unsigned long long nextTrackPointer = midiFile.tellg();
    nextTrackPointer += trackLength ;
    return nextTrackPointer;
  }
  void setTrack(unsigned long long index){
    midiFile.seekg(trackPointers.at(index));
  }
  void readTrack(){
    std:: cout << std::string(30, '_') << std::endl;
    midiFile.read(reinterpret_cast<char *>(four), 4);
    std::cout << "Chunk Type: "
              << four[0]
              << four[1]
              << four[2]
              << four[3]
         << std::endl;
    midiFile.read(reinterpret_cast<char *>(four), 4);
    std::cout << "Chunk Length: " << as32uint(four) << std::endl;
    const unsigned int trackLength = as32uint(four) ;
    const unsigned long long trackStart = midiFile.tellg();

    while(midiFile.tellg() < trackStart + trackLength){
      std:: cout << std::string(30, '_') << std::endl;
      readEvent();
    }
  }
  void readEvent(){
    std::stack<unsigned char> stk{};
    do {
      midiFile.read(reinterpret_cast<char *>(&one), 1);
      stk.push(one);
    } while((one & 0b10000000)) ;
    unsigned int length = unstackVariableNumber(stk);
    std::cout << "Delta Time: " << length << std::endl;

    //read midi event
    //status byte
    midiFile.read(reinterpret_cast<char *>(&one), 1);
    const unsigned char status = one ;

    if(status & 0x80){
      std::cout << "Midi Event Status Code: " ;
      std::cout << std::hex << +status << " " << std::endl;
    }
    if(status == 0xF0){
      std::cout << "SysEx Event Length: " ;
      //length is variable
      std::stack<unsigned char> stk{};
      do {
        midiFile.read(reinterpret_cast<char *>(&one), 1);
        stk.push(one);
        std::cout << std::hex << +one << " ";
      } while((one & 0b10000000)) ;
      std::cout << std::endl;
      unsigned int length = unstackVariableNumber(stk);
      std::cout << length << std::endl;
      //skip SysEx event data
      for(unsigned char i=0;i<length;i++){
        midiFile.get();
      }
    }
    else if(status == 0xF2){
        std::cout << "Song Position Pointer" << std::endl;
        //advance 2 bytes, skip
        midiFile.get();
        midiFile.get();
    }
    else if(status == 0xF3){
        std::cout << "Song Select" << std::endl;
        //advance 1 byte, skip
        midiFile.get();
    }
    else if(status == 0xFF){
        std::cout << "Meta Event" << std::endl;
        //advance 1 byte
        const unsigned char type = midiFile.get();
        std::cout << "Meta Event Type: " << metaEventTypes[type] << std::endl;
        //length is variable
        std::stack<unsigned char> stk{};
        do {
          midiFile.read(reinterpret_cast<char *>(&one), 1);
          stk.push(one);
        } while((one & 0b10000000)) ;
        unsigned int length = unstackVariableNumber(stk);
        std::cout << "Length: " << length << std::endl;
        //Meta event data
        std::vector<char> data{};
        for(unsigned char i=0;i<length;i++){
          const char c = midiFile.get();
          data.push_back(c);
        }
        std::cout << "Data(text): " ;
        for(auto c: data) std::cout << c;

        std::cout << std::endl << "Data(hex): " ;
        for(auto c: data) std::cout << std::hex << std::setw(2)
                                    << std::setfill('0')
                                    << +((unsigned char)c)
                                    << " " ;

        std::cout << std::endl;
    }
    else if(status > 0xF0){
        //no data bytes, goto next midi event
    }
    else {
      //read data bytes
      unsigned char op = status & 0xF0 ;
      unsigned char channel = status & 0x0F;
      if(!(op & 0b10000000)){
        op = lastOp ;
        channel = lastChannel ;
        //push back the status byte into ifstream midiFile
        midiFile.unget();
      }
      std::cout << "Channel: " << +channel << std::endl;
      switch(op){
        case 0b10000000: std::cout << "Note Off" << std::endl;
          noteOff();
          lastOp = op;
          lastChannel = channel;
          break;
        case 0b10010000: std::cout << "Note On" << std::endl;
          noteOn();
          lastOp = op;
          lastChannel = channel;
          break;
        case 0b10100000: std::cout << "Polyphonic Key Pressure, Aftertouch" << std::endl;
          pkp();
          lastOp = op;
          lastChannel = channel;
          break;
        case 0b10110000: std::cout << "Control Change" << std::endl;
          controlChange();
          lastOp = op;
          lastChannel = channel;
          break;
        case 0b11000000: std::cout << "Program Change" << std::endl;
          programChange();
          lastOp = op;
          lastChannel = channel;
          break;
        case 0b11010000: std::cout << "Channel Pressure, Aftertouch" << std::endl;
          channelPressure();
          lastOp = op;
          lastChannel = channel;
          break;
        case 0b11100000: std::cout << "Pitch Wheel Change" << std::endl;
          pitchWheelChange();
          lastOp = op;
          lastChannel = channel;
          break;
//        case 0b10110000: std::cout << "Channel Mode Messages" << std::endl;
//          midiFile.get(); midiFile.get();
//          break;
        default : std::cout << "Unknown operation: " << std::hex << +status << std::endl;
      }
    }
  }
  void noteOff(){
    //take 2 bytes data
    const unsigned char data1 = midiFile.get();
    const unsigned char data2 = midiFile.get();
    std::cout << "    Note Number: " << +data1
              << std::endl;
    std::cout << "    Velocity: " << +data2
              << std::endl;
  }
  void noteOn(){
    //take 2 bytes data
    const unsigned char data1 = midiFile.get();
    const unsigned char data2 = midiFile.get();
    std::cout << "    Note Number: " << +data1
              << std::endl;
    std::cout << "    Velocity: " << +data2
              << std::endl;
  }
  void controlChange(){
    //take 2 bytes data
    const unsigned char data1 = midiFile.get();
    const unsigned char data2 = midiFile.get();
    std::cout << "    Control Change: " << +data1
              << std::endl;
    std::cout << "    Value: " << +data2
              << std::endl;
  }
  void pkp(){
    //take 2 bytes data
    const unsigned char data1 = midiFile.get();
    const unsigned char data2 = midiFile.get();
    std::cout << "    Key: " << +data1
              << std::endl;
    std::cout << "    Value: " << +data2
              << std::endl;
  }
  void programChange(){
    //take 1 byte data
    const unsigned char data1 = midiFile.get();
    std::cout << "    program Change: " << +data1
              << std::endl;
  }
  void channelPressure(){
    //take 1 byte data
    const unsigned char data1 = midiFile.get();
    std::cout << "    Value: " << +data1
              << std::endl;
  }
  void pitchWheelChange(){
    //take 2 bytes data
    const unsigned char data1 = midiFile.get();
    const unsigned char data2 = midiFile.get();
    std::cout << "    Least significant 7bits: " << +data1
              << std::endl;
    std::cout << "    Most significant 7bits: " << +data2
              << std::endl;
  }
};

int main(int argc, char ** argv){
  if(argc != 2){
    std::cerr << "Wrong number of arguments: command <filename> "
              << std::endl;
    exit(1);
  }
  MidiReader midi(argv[1]);

  std::cout << "Which track would you like to read?<track number> ";
  unsigned long long trackIndex;
  std::cin >> trackIndex;

  midi.setTrack(trackIndex);
  midi.readTrack();

  return 0;
}
