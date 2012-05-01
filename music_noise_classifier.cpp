#include "adaboost.h"
#include "audio_clip.h"
#include "audiofilereader.h"
#include "assert.h"
#include "constant.h"
#include "hmm_smoother.h"
#include "feature_extractor.h"
#include "music_noise_classifier.h"

#include <iostream>
using namespace std;

Feature_extractor fe_mnc;
Adaboost ada;
HMM_smoother hmms;

bool MNC_DEBUG_FLAG = false;

void Music_noise_classifier::load_adaboost_paras(const char* f_ada, const char* f_em) {
#ifdef DEBUG
    cout << "Loading AdaBoost parameters..." << endl;
#endif
	ada.load_classifier(f_ada);
	ada.load_eigenmusic_inv(f_em);
}

void Music_noise_classifier::do_music_noise_classify(const char *filename, const char* f_ada, const char* f_em, vector<Audio_clip> &clips) {
	vector<int> pred_result, result_no_smooth, result_with_smooth;
	vector<float> ada_raw_result;
    
    load_adaboost_paras(f_ada, f_em);

	do_adaboost(filename, pred_result, ada_raw_result);

	hmms.do_smooth(ada_raw_result, result_with_smooth);

	do_gen_clips(filename, result_with_smooth, clips);
}

void Music_noise_classifier::do_adaboost(const char *filename, vector<int> &pred_result, vector<float> &ada_raw_result) {
	int i, count = 0;
	Audio_file_reader reader;
	vector<float> aver_spec, spectrum;
	float ada_raw;
	//no overlapping
	fe_mnc.set_parameters(SAMPLES_PER_FRAME / RESAMPLE_FREQ, SAMPLES_PER_FRAME / RESAMPLE_FREQ);
	/*
     *****************************FOR SIMPLICITY**************************
     
    reader.open(filename, fe_mnc, RESAMPLE_FREQ, MNC_DEBUG_FLAG);
    */
    
    string t = "resample_" + string(filename);
    reader.open(t.c_str(), fe_mnc, 0, MNC_DEBUG_FLAG);
    
    //init aver_spec
    aver_spec.assign(SAMPLES_PER_FRAME / 2 + 1, 0);
	
	while (1) {
		if (count != 0 && count % NUM_AVER == 0) {

			// take the average of NUM_AVER windows
			for (i = 0; i < aver_spec.size(); i++) {
				aver_spec[i] /= NUM_AVER;
			}
            pred_result.push_back(ada.do_prediction(aver_spec, &ada_raw));
			ada_raw_result.push_back(ada_raw);
			// reassign the average to 0
			aver_spec.assign(SAMPLES_PER_FRAME / 2 + 1, 0);
		}

		if (fe_mnc.get_spectrum(reader, spectrum, MNC_DEBUG_FLAG)) {            
#ifdef DEBUG
            cout << aver_spec.size() << endl;
            cout << spectrum.size() << endl;
            cout << "frame # " << count << endl;
			assert(aver_spec.size() == spectrum.size());
			assert(spectrum.size() == SAMPLES_PER_FRAME / 2 + 1);
#endif
			for (i = 0; i < aver_spec.size(); i++) {
				aver_spec[i] += spectrum[i]; 
			}
			count++;
		} else {
            cout << "the last un-filled frame" << endl;
			// handle the last frame, if any
			if (count % NUM_AVER != 0) {
				for (i = 0; i < aver_spec.size(); i++) {
					aver_spec[i] /= count % NUM_AVER;
				}
				pred_result.push_back(ada.do_prediction(aver_spec, &ada_raw));
				ada_raw_result.push_back(ada_raw);
			}
			break;
		}
	}
}

void Music_noise_classifier::do_gen_clips(const char* filename, vector<int> result_with_smooth, vector<Audio_clip> &clips) {
	bool new_clip_flag = true;
	int cur;
	for (int i = 0; i < result_with_smooth.size(); ) {
        Audio_clip clip(filename);
		if (new_clip_flag) {
			clip.set_start(i);
			clip.is_music = (result_with_smooth[i] == 1);
			cur = result_with_smooth[i];
			i++;
			new_clip_flag = false;
			continue;
		}
		if (result_with_smooth[i] != cur) {
			clip.set_end(i-1);
			if (clip.is_music && clip.get_end() - clip.get_start() >= int(MIN_LENGTH_SECS / (NUM_AVER * SAMPLES_PER_FRAME / RESAMPLE_FREQ) + 0.5)) {
				clips.push_back(clip);
			}
			new_clip_flag = true;
			continue;
		} 
		i++;
	}
}

