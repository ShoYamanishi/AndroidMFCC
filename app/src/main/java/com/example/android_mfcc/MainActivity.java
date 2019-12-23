package com.example.android_mfcc;

import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;
import android.widget.TextView;

import java.util.Random;
import java.util.TimerTask;
import java.util.Timer;

public class MainActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        final ScrollingHeatMapView mfccView = findViewById(R.id.mfcc_heatmap_view);
        final ScrollingHeatMapView fftView = findViewById(R.id.fft_heatmap_view);
        mfccView.setResolution ( 200,  27 );
        mfccView.setColorType(1);
        fftView.setResolution  ( 200, 256 );

        mMFCCProcessor = new TopLevelMFCCProcessor( mfccView, fftView );
        mAudioReceiver = new AudioReceiver( mMFCCProcessor );
    }

    TopLevelMFCCProcessor mMFCCProcessor;
    AudioReceiver         mAudioReceiver;
}
