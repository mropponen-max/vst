#include "PluginProcessor.h"
#include <cstdio>
int main(int argc, char** argv){
  juce::ScopedJuceInitialiser_GUI init;
  MultiRubyAudioProcessor p; p.setPlayConfigDetails(2,2,48000,512); p.prepareToPlay(48000,512);
  std::unique_ptr<juce::AudioProcessorEditor> ed(p.createEditor());
  ed->setSize(argc>2?atoi(argv[2]):1100, argc>3?atoi(argv[3]):700);
  auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
  juce::File f(argv[1]); f.deleteFile();
  juce::PNGImageFormat png; juce::FileOutputStream os(f); png.writeImageToStream(img, os);
  std::printf("wrote %s\n", argv[1]);
}
