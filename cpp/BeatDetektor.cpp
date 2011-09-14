
/*
 *  BeatDetektor.cpp
 *
 *  BeatDetektor - CubicFX Visualizer Beat Detection & Analysis Algorithm
 *
 *  Created by Charles J. Cliffe <cj@cubicproductions.com> on 09-11-30.
 *  Copyright 2009 Charles J. Cliffe. All rights reserved.
 *
 *  BeatDetektor is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  BeatDetektor is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Visit www.cubicvr.org for BeatDetektor forum and support.
 *
 */


#include "BeatDetektor.h"

void BeatDetektor::process(float timer_seconds, std::vector<float> &fft_data)
{
	if (!last_timer) { last_timer = timer_seconds; return; }	// ignore 0 start time
	
	if (timer_seconds < last_timer) { reset(); return; }
	
	float timestamp = timer_seconds;
	
	last_update = timer_seconds - last_timer;
	last_timer = timer_seconds;
	
	total_time+=last_update;
	
	unsigned int range_step = (fft_data.size()/BD_DETECTION_RANGES);
	unsigned int range = 0;
	int i,x;
	float v;
	
	float bpm_floor = 60.0/BPM_MAX;
	float bpm_ceil = 60.0/BPM_MIN;
	
	if (current_bpm != current_bpm) current_bpm = 0;
	
	for (x=0; x<fft_data.size(); x+=range_step)
	{
		if (!src)
		{
			a_freq_range[range] = 0;
			
			// accumulate frequency values for this range
			for (i = x; i<x+range_step; i++)
			{
				v = fabs(fft_data[i]);
				a_freq_range[range] += v;
			}
			
			// average for range
			a_freq_range[range] /= range_step;
			
			// two sets of averages chase this one at a 
			
			// moving average, increment closer to a_freq_range at a rate of 1.0 / detection_rate seconds
			ma_freq_range[range] -= (ma_freq_range[range]-a_freq_range[range])*last_update*detection_rate;
			// moving average of moving average, increment closer to ma_freq_range at a rate of 1.0 / detection_rate seconds
			maa_freq_range[range] -= (maa_freq_range[range]-ma_freq_range[range])*last_update*detection_rate;
		}
		else
		{
			a_freq_range[range] = src->a_freq_range[range];
			ma_freq_range[range] = src->ma_freq_range[range];
			maa_freq_range[range] = src->maa_freq_range[range];
		}
		
		
		// if closest moving average peaks above trailing (with a tolerance of BD_DETECTION_FACTOR) then trigger a detection for this range 
		bool det = (ma_freq_range[range]*detection_factor >= maa_freq_range[range]);
		
		// compute bpm clamps for comparison to gap lengths
		
		// clamp detection averages to input ranges
		if (ma_bpm_range[range] > bpm_ceil) ma_bpm_range[range] = bpm_ceil;
		if (ma_bpm_range[range] < bpm_floor) ma_bpm_range[range] = bpm_floor;
		if (maa_bpm_range[range] > bpm_ceil) maa_bpm_range[range] = bpm_ceil;
		if (maa_bpm_range[range] < bpm_floor) maa_bpm_range[range] = bpm_floor;
		
		bool rewarded = false;
		
		// new detection since last, test it's quality
		if (!detection[range] && det)
		{
			// calculate length of gap (since start of last trigger)
			float trigger_gap = timestamp-last_detection[range];
			
#define REWARD_VALS 7
			float reward_tolerances[REWARD_VALS] = { 0.001, 0.005, 0.01, 0.02, 0.04, 0.08, 0.10  };  
			float reward_multipliers[REWARD_VALS] = { 20.0, 10.0, 8.0, 1.0, 1.0/2.0, 1.0/4.0, 1.0/8.0 };
			
			// trigger falls within acceptable range, 
			if (trigger_gap < bpm_ceil && trigger_gap > (bpm_floor))
			{		
				// compute gap and award quality
				
				for (i = 0; i < REWARD_VALS; i++)
				{
					if (fabs(ma_bpm_range[range]-trigger_gap) < ma_bpm_range[range]*reward_tolerances[i])
					{
						detection_quality[range] += quality_reward * reward_multipliers[i]; 
						rewarded = true;
#if DEVTEST_BUILD
						//						printf("1/1\n");
						contribution_counter[1]++;
#endif
						
					}
				}
				
				
				if (rewarded) 
				{
					last_detection[range] = timestamp;
				}
			}
			else if (trigger_gap >= bpm_ceil) // low quality, gap exceeds maximum time
			{
				// test for 2* beat
				trigger_gap /= 2.0;
				// && fabs((60.0/trigger_gap)-(60.0/ma_bpm_range[range])) < 50.0
				if (trigger_gap < bpm_ceil && trigger_gap > (bpm_floor)) for (i = 0; i < REWARD_VALS; i++)
				{
					if (fabs(ma_bpm_range[range]-trigger_gap) < ma_bpm_range[range]*reward_tolerances[i])
					{
						detection_quality[range] += quality_reward * reward_multipliers[i]; 
						rewarded = true;
#if DEVTEST_BUILD
						//						printf("2/1\n");
						contribution_counter[2]++;
#endif
					}
				}
				
				if (!rewarded) trigger_gap *= 2.0;
				
				// start a new gap test, next gap is guaranteed to be longer
				last_detection[range] = timestamp;					
			}
			
			
			float qmp = (detection_quality[range]/quality_avg)*BD_QUALITY_STEP;
			if (qmp > 1.0)
			{
				qmp = 1.0;
			}
			
			if (rewarded)
			{
				ma_bpm_range[range] -= (ma_bpm_range[range]-trigger_gap) * qmp;
				maa_bpm_range[range] -= (maa_bpm_range[range]-ma_bpm_range[range]) * qmp;
			}
			else if (trigger_gap >= bpm_floor && trigger_gap <= bpm_ceil)
			{
				if (detection_quality[range] < quality_avg*BD_QUALITY_TOLERANCE && current_bpm)
				{
					ma_bpm_range[range] -= (ma_bpm_range[range]-trigger_gap) * BD_QUALITY_STEP;
					maa_bpm_range[range] -= (maa_bpm_range[range]-ma_bpm_range[range]) * BD_QUALITY_STEP;
				}
				detection_quality[range] -= BD_QUALITY_STEP;
			}
			else if (trigger_gap >= bpm_ceil)
			{
				if (detection_quality[range] < quality_avg*BD_QUALITY_TOLERANCE && current_bpm)
				{
					ma_bpm_range[range] -= (ma_bpm_range[range]-current_bpm) * 0.5;
					maa_bpm_range[range] -= (maa_bpm_range[range]-ma_bpm_range[range]) * 0.5;
				}
				detection_quality[range] -= quality_reward*BD_QUALITY_STEP;
			}
			
		}
		
		if ((!rewarded && timestamp-last_detection[range] > bpm_ceil) || ((det && fabs(ma_bpm_range[range]-current_bpm) > bpm_offset))) 
			detection_quality[range] -= detection_quality[range]*BD_QUALITY_STEP*quality_decay*last_update;
		
		// quality bottomed out, set to 0
		if (detection_quality[range] <= 0) detection_quality[range]=0.001;
		
		
		detection[range] = det;		
		
		range++;
	}
	
	
	// total contribution weight
	quality_total = 0;
	
	// total of bpm values
	float bpm_total = 0;
	// number of bpm ranges that contributed to this test
	int bpm_contributions = 0;
	
	
	// accumulate quality weight total
	for (x=0; x<BD_DETECTION_RANGES; x++)
	{
		quality_total += detection_quality[x];
	}
	
	// determine the average weight of each quality range
	quality_avg = quality_total / (float)BD_DETECTION_RANGES;
	
	
	ma_quality_avg += (quality_avg - ma_quality_avg) * last_update * detection_rate/2.0;
	maa_quality_avg += (ma_quality_avg - maa_quality_avg) * last_update;
	ma_quality_total += (quality_total - ma_quality_total) * last_update * detection_rate/2.0;
	
	ma_quality_avg -= 0.98*ma_quality_avg*last_update*3.0;
	
	if (ma_quality_total <= 0) ma_quality_total = 1.0;
	if (ma_quality_avg <= 0) ma_quality_avg = 1.0;
	
	float avg_bpm_offset = 0.0;
	float offset_test_bpm = current_bpm;
	std::map<int,float> draft;
	std::map<int,float> fract_draft;
	
	{
		for (x=0; x<BD_DETECTION_RANGES; x++)
		{
			// if this detection range weight*tolerance is higher than the average weight then add it's moving average contribution 
			if (detection_quality[x]*BD_QUALITY_TOLERANCE >= ma_quality_avg)
			{
				if (maa_bpm_range[x] < bpm_ceil && maa_bpm_range[x] > bpm_floor)
				{
					bpm_total += maa_bpm_range[x];
					
					float draft_float = round((60.0/maa_bpm_range[x])*1000.0);
					
					draft_float = (fabs(ceil(draft_float)-(60.0/current_bpm)*1000.0)<(fabs(floor(draft_float)-(60.0/current_bpm)*1000.0)))?ceil(draft_float/10.0):floor(draft_float/10.0);
					
					float draft_int = (int)(draft_float/10.0);
					
					draft[draft_int]+= (detection_quality[x]/quality_avg);
					bpm_contributions++;
					if (offset_test_bpm == 0.0) offset_test_bpm = maa_bpm_range[x];
					else 
					{
						avg_bpm_offset += fabs(offset_test_bpm-maa_bpm_range[x]);
					}
					
				}
			}
		}
	}
	
	// if we have one or more contributions that pass criteria then attempt to display a guess
	bool has_prediction = (bpm_contributions>=minimum_contributions)?true:false;
	
	
	std::map<int,float>::iterator draft_i;
	
	if (has_prediction) 
	{
		
		int draft_winner=0;
		int win_max = 0;
		
		for (draft_i = draft.begin(); draft_i != draft.end(); draft_i++)
		{
			if ((*draft_i).second > win_max)
			{
				win_max = (*draft_i).second;
				draft_winner = (*draft_i).first;
			}
		}
		
		bpm_predict = (60.0/(float)(draft_winner/10.0));
		
		avg_bpm_offset /= (float)bpm_contributions;
		bpm_offset = avg_bpm_offset;
		
		if (!current_bpm)  
		{
			current_bpm = bpm_predict; 
		}
		
		
		if (current_bpm && bpm_predict) current_bpm -= (current_bpm-bpm_predict)*last_update; //*avg_bpm_offset*200.0;	
		if (current_bpm != current_bpm || current_bpm < 0) current_bpm = 0;
		
		
		// hold a contest for bpm to find the current mode
		std::map<int,float>::iterator contest_i;
		
		float contest_max=0;
		
		for (contest_i = bpm_contest.begin(); contest_i != bpm_contest.end(); contest_i++)
		{
			if (contest_max < (*contest_i).second) contest_max =(*contest_i).second; 
			if (((*contest_i).second) > BD_FINISH_LINE/2.0)
			{
				bpm_contest_lo[round((float)((*contest_i).first)/10.0)]+= (((*contest_i).second)/10.0)*last_update;
			}
		}
		
		
		// normalize to a finish line of BD_FINISH_LINE
		if (contest_max > finish_line) 
		{
			for (contest_i = bpm_contest.begin(); contest_i != bpm_contest.end(); contest_i++)
			{
				(*contest_i).second=((*contest_i).second/contest_max)*finish_line;
			}
		}
		
		contest_max = 0;
		
		for (contest_i = bpm_contest_lo.begin(); contest_i != bpm_contest_lo.end(); contest_i++)
		{
			if (contest_max < (*contest_i).second) contest_max =(*contest_i).second; 
		}
		
		if (contest_max > finish_line) 
		{
			for (contest_i = bpm_contest_lo.begin(); contest_i != bpm_contest_lo.end(); contest_i++)
			{
				(*contest_i).second=((*contest_i).second/contest_max)*finish_line;
			}
		}
		
		
		// decay contest values from last loop
		for (contest_i = bpm_contest.begin(); contest_i != bpm_contest.end(); contest_i++)
		{
			(*contest_i).second-=(*contest_i).second*(last_update/detection_rate);
		}
		
		// decay contest values from last loop
		for (contest_i = bpm_contest_lo.begin(); contest_i != bpm_contest_lo.end(); contest_i++)
		{
			(*contest_i).second-=(*contest_i).second*(last_update/detection_rate);
		}
		
		
		bpm_timer+=last_update;
		
		int winner = 0;
		int winner_lo = 0;				
		
		// attempt to display the beat at the beat interval ;)
		if (bpm_timer > winning_bpm/4.0 && current_bpm)
		{		
			if (winning_bpm) while (bpm_timer > winning_bpm/4.0) bpm_timer -= winning_bpm/4.0;
			
			// increment beat counter
			
			quarter_counter++;		
			half_counter= quarter_counter/2;
			beat_counter = quarter_counter/4;
			
			// award the winner of this iteration
			bpm_contest[(int)round((60.0/current_bpm)*10.0)]+=quality_reward;
			
			win_val = 0;
			
			// find the overall winner so far
			for (contest_i = bpm_contest.begin(); contest_i != bpm_contest.end(); contest_i++)
			{
				if (win_val < (*contest_i).second)
				{
					winner = (*contest_i).first;
					win_val = (*contest_i).second;
				}
			}
			
			if (winner)
			{
				win_bpm_int = winner;
				winning_bpm = 60.0/(float)(winner/10.0);
			}
			
			
			win_val_lo = 0;		
			
			// find the overall winner so far
			for (contest_i = bpm_contest_lo.begin(); contest_i != bpm_contest_lo.end(); contest_i++)
			{
				if (win_val_lo < (*contest_i).second)
				{
					winner_lo = (*contest_i).first;
					win_val_lo = (*contest_i).second;
				}
			}
			
			if (winner_lo)
			{
				win_bpm_int_lo = winner_lo;
				winning_bpm_lo = 60.0/(float)(winner_lo);
			}
#if DEVTEST_BUILD
			if (debugmode && ((quarter_counter % 4) == 0)) 
			{
				printf("[%0.0f-%0.0f] quality: %0.2f / %0.2f percent",BPM_MIN,BPM_MAX,quality_total,(quality_total/(ma_quality_avg*(float)BD_DETECTION_RANGES))*50.0);
				printf(", current bpm estimate: %d @ %0.2f / %0.5f",winner,win_val,bpm_offset);
				printf("  low res estimate: %d @ %0.2f\n",winner_lo,win_val_lo);
				std::map<int, int>::iterator contrib_i;
				
				printf("contrib: ");
				for (contrib_i = contribution_counter.begin(); contrib_i !=  contribution_counter.end(); contrib_i++)
				{
					printf("%d: %d \t",(*contrib_i).first,(*contrib_i).second);
				}
				printf("\n");
			}
#endif
		}
	}
}
