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
  // Derive screenshot dimensions from the actual editor size. The old fixed
  // 1230x660 / 820x440 captures added vertical padding that the plugin window
  // does not have, making the real UI look artificially short in screenshots.
  const int defaultWidth=editor.getWidth();
  const int defaultHeight=editor.getHeight();
  if(defaultWidth<=0 || defaultHeight<=0) {
      std::fprintf(stderr,"Rack editor reported an invalid default size\\n");
      return 2;
  }
  const int compactWidth=juce::roundToInt((float)defaultWidth*(2.0f/3.0f));
  const int compactHeight=juce::roundToInt((float)defaultHeight*(2.0f/3.0f));
  options.scenes={
      {"compact-normal",compactWidth,compactHeight,0,0,false},
      {"compact-tour",compactWidth,compactHeight,0,1,false},
      {"default-normal",defaultWidth,defaultHeight,0,0,false},
      {"default-tour",defaultWidth,defaultHeight,0,1,false}
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
