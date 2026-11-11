// Actual ModularFXSuite editor, no mock controls or audio-hardware dependency.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <SkellCore/HeadlessUIRenderer.h>
#include <cstdio>
int main(int argc,char** argv){
  juce::ScopedJuceInitialiser_GUI gui;
  const auto path=juce::File::getCurrentWorkingDirectory().getChildFile(
      argc>=3 && juce::String(argv[1])=="--output" ? argv[2] : "ui-tour-screenshots");
  ModularFXAudioProcessor processor;
  processor.prepareToPlay(48000.0,512);
  ModularFXAudioProcessorEditor editor(processor);
  editor.dismissTourForScreenshot();
  skellcore::ui::headless::Options options;
  options.project="Skell Rack actual JUCE GuidedTour";
  options.scenes={
      {"compact-normal",820,440,0,0,false},
      {"compact-tour",820,440,0,1,false},
      {"default-normal",1230,660,0,0,false},
      {"default-tour",1230,660,0,1,false}
  };
  int failures=0;
  options.configure=[&](const skellcore::ui::headless::Scene& scene){
      editor.dismissTourForScreenshot();
      if(scene.mode==1 && !editor.replayTourForScreenshot()){
          std::fprintf(stderr,"Rack tour not visible on actual editor\\n");
          ++failures;
      }
  };
  options.mustDiffer={
      {"compact-normal","compact-tour"},
      {"default-normal","default-tour"}
  };
  const auto report=skellcore::ui::headless::render(editor,path,options);
  for(const auto& error:report.errors)
      std::fprintf(stderr,"Rack guided tour screenshot: %s\\n",error.toRawUTF8());
  editor.dismissTourForScreenshot();
  processor.releaseResources();
  return failures==0 && report.passed() && report.captured==4 ? 0:1;
}
