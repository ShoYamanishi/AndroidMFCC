package com.example.android_mfcc;

import java.util.LinkedList;
import java.util.List;
import java.util.ListIterator;

public class AudioChunkAggregator {

    public synchronized void putChunk( short[] chunk ){
        mChunksNewToOld.addLast( chunk );
        mTotalNumSamples += chunk.length;
    }

    public synchronized int totalNumSamples() { return mTotalNumSamples; }

    public synchronized float[] getConsecutive400SamplesInfloat() {

        if (mTotalNumSamples < 400) {
            return null;
        }

        float[] arrayOut = new float[400];

        ListIterator<short[]> iter = mChunksNewToOld.listIterator(0);
        int writePos = 0;
        int numChunksToDelete = 0;
        boolean Done = false;

        while ( !Done ) {

            short[] curChunk = iter.next();
            int readPos = mNextReadPosInCurChunk;
            while ( readPos < curChunk.length ) {

                arrayOut[writePos] = (float)( curChunk[ readPos ] );
                readPos++;
                writePos++;
                mTotalNumSamples--;

                if ( writePos == 400 ) {
                    Done = true;
                    break;
                }
            }

            if ( readPos == curChunk.length ) {
                mNextReadPosInCurChunk = 0;
                numChunksToDelete++;
            }
            else {
                mNextReadPosInCurChunk = readPos;
            }
        }

        for ( int i = 0; i < numChunksToDelete; i++ ) {
            mChunksNewToOld.removeFirst();
        }

        return arrayOut;
    }

    private LinkedList< short[] > mChunksNewToOld = new LinkedList< short[] >();

    private int mTotalNumSamples       = 0;
    private int mNextReadPosInCurChunk = 0;

}
