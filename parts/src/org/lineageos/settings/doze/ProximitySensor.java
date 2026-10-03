/*
 * Copyright (C) 2015 The CyanogenMod Project
 *               2017-2018 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.lineageos.settings.doze;

import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.util.Log;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;

public class ProximitySensor implements SensorEventListener {
    private static final boolean DEBUG = false;
    private static final String TAG = "ProximitySensor";

    // Maximum time for the hand to cover the sensor: 1s
    private static final int HANDWAVE_MAX_DELTA_NS = 1000 * 1000 * 1000;

    // Minimum time until the device is considered to have been in the pocket: 2s
    private static final int POCKET_MIN_DELTA_NS = 2000 * 1000 * 1000;

    private SensorManager mSensorManager;
    private Sensor mSensor;
    private Context mContext;
    private ExecutorService mExecutorService;

    private boolean mActive = false;
    private boolean mSawNear = false;
    private long mInPocketTime = 0;

    public ProximitySensor(Context context) {
        mContext = context;
        mSensorManager = mContext.getSystemService(SensorManager.class);
        mSensor = mSensorManager != null
                ? mSensorManager.getDefaultSensor(Sensor.TYPE_PROXIMITY, false) : null;
        mExecutorService = Executors.newSingleThreadExecutor();
    }

    private Future<?> submit(Runnable runnable) { return mExecutorService.submit(runnable); }

    @Override
    public void onSensorChanged(SensorEvent event) {
        synchronized (this) {
            if (!mActive || mSensor == null) {
                return;
            }

            if (DEBUG)
                Log.d(TAG, "Got sensor event: " + event.values[0]);

            boolean isNear = event.values[0] < mSensor.getMaximumRange();
            if (!mSawNear && isNear) {
                // Keep the start of the near interval; later samples must not reset it.
                mInPocketTime = event.timestamp;
            } else if (mSawNear && !isNear && shouldPulse(event.timestamp)) {
                DozeUtils.wakeOrLaunchDozePulse(mContext);
            }
            mSawNear = isNear;
        }
    }

    private boolean shouldPulse(long timestamp) {
        long delta = timestamp - mInPocketTime;

        if (DozeUtils.isHandwaveGestureEnabled(mContext)
                && DozeUtils.isPocketGestureEnabled(mContext)) {
            return true;
        } else if (DozeUtils.isHandwaveGestureEnabled(mContext)) {
            return delta < HANDWAVE_MAX_DELTA_NS;
        } else if (DozeUtils.isPocketGestureEnabled(mContext)) {
            return delta >= POCKET_MIN_DELTA_NS;
        }
        return false;
    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {
        /* Empty */
    }

    private void resetState() {
        mSawNear = false;
        mInPocketTime = 0;
    }

    protected void enable() {
        if (DEBUG)
            Log.d(TAG, "Enabling");
        submit(() -> {
            if (mSensorManager == null || mSensor == null) {
                synchronized (this) {
                    mActive = false;
                    resetState();
                }
                return;
            }

            synchronized (this) {
                resetState();
                mActive = true;
            }
            if (!mSensorManager.registerListener(
                    this, mSensor, SensorManager.SENSOR_DELAY_NORMAL)) {
                synchronized (this) {
                    mActive = false;
                    resetState();
                }
            }
        });
    }

    protected void disable() {
        if (DEBUG)
            Log.d(TAG, "Disabling");
        synchronized (this) {
            mActive = false;
            resetState();
        }
        submit(() -> {
            if (mSensorManager != null) {
                mSensorManager.unregisterListener(this);
            }
            synchronized (this) {
                mActive = false;
                resetState();
            }
        });
    }

    protected void close() {
        if (!mExecutorService.isShutdown()) {
            synchronized (this) {
                mActive = false;
                resetState();
            }
            submit(() -> {
                if (mSensorManager != null) {
                    mSensorManager.unregisterListener(this);
                }
                synchronized (this) {
                    mActive = false;
                    resetState();
                }
            });
            mExecutorService.shutdown();
        }
    }
}
