/*
 * Copyright (C) 2017, The LineageOS Project
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

package com.android.internal.telephony;

import android.content.Context;
import android.os.AsyncResult;
import android.os.Message;
import android.os.Parcel;
import android.service.carrier.CarrierIdentifier;

import com.android.internal.telephony.uicc.SimPhoneBookAdnRecord;

import java.util.Arrays;
import java.util.List;

import static com.android.internal.telephony.RILConstants.*;

/**
 * RIL customization for ether
 */

public class Axon7RIL extends RIL {
    static final String LOG_TAG = "Axon7RIL";

    final int RIL_REQUEST_SIM_GET_ATR_LEGACY = 136;
    final int RIL_REQUEST_CAF_SIM_OPEN_CHANNEL_WITH_P2_LEGACY = 137;
    final int RIL_REQUEST_GET_ADN_RECORD_LEGACY = 138;
    final int RIL_REQUEST_UPDATE_ADN_RECORD_LEGACY = 139;

    final int RIL_UNSOL_RESPONSE_ADN_INIT_DONE_LEGACY = 1046;
    final int RIL_UNSOL_RESPONSE_ADN_RECORDS_LEGACY = 1047;

    public Axon7RIL(Context context, int preferredNetworkType, int cdmaSubscription) {
        this(context, preferredNetworkType, cdmaSubscription, null);
    }

    public Axon7RIL(Context context, int preferredNetworkType,
                    int cdmaSubscription, Integer instanceId) {
        super(context, preferredNetworkType, cdmaSubscription, instanceId);
    }

    protected RILRequest processSolicited (Parcel p, int type) {
        boolean found = false;
        RILRequest rr = null;
        int newRequest = 0;

        int dataPosition = p.dataPosition(); // save off position within the Parcel
        int serial = p.readInt();
        int error = p.readInt();

        // Pre-process the reply before popping it
        synchronized (mRequestList) {
            RILRequest tr = mRequestList.get(serial);
            if (tr != null && tr.mSerial == serial) {
                if (error == 0 || p.dataAvail() > 0) {
                    try {
                        switch (tr.mRequest) {
                            // Get those we're interested in
                        case RIL_REQUEST_SIM_GET_ATR_LEGACY:
                        case RIL_REQUEST_CAF_SIM_OPEN_CHANNEL_WITH_P2_LEGACY:
                        case RIL_REQUEST_GET_ADN_RECORD_LEGACY:
                        case RIL_REQUEST_UPDATE_ADN_RECORD_LEGACY:
                            rr = tr;
                            break;
                        }
                    } catch (Throwable thr) {
                        // Exceptions here usually mean invalid RIL responses
                        if (tr.mResult != null) {
                            AsyncResult.forMessage(tr.mResult, null, thr);
                            tr.mResult.sendToTarget();
                        }
                        return tr;
                    }
                }
            }
        }

        if (rr == null) {
            // Nothing we care about, go up
            p.setDataPosition(dataPosition);
            return super.processSolicited(p, type);
        }

        rr = findAndRemoveRequestFromList(serial);
        if (rr == null) {
            return rr;
        }

        Object ret = null;
        if (error == 0 || p.dataAvail() > 0) {
            switch (rr.mRequest) {
            case RIL_REQUEST_SIM_GET_ATR_LEGACY:
                ret = responseString(p);
                newRequest = RIL_REQUEST_SIM_GET_ATR;
                break;
            case RIL_REQUEST_CAF_SIM_OPEN_CHANNEL_WITH_P2_LEGACY:
                newRequest = RIL_REQUEST_CAF_SIM_OPEN_CHANNEL_WITH_P2;
                ret = responseInts(p);
                break;
            case RIL_REQUEST_GET_ADN_RECORD_LEGACY:
                newRequest = RIL_REQUEST_GET_ADN_RECORD;
                ret = responseInts(p);
                break;
            case RIL_REQUEST_UPDATE_ADN_RECORD_LEGACY:
                ret = responseInts(p);
                newRequest = RIL_REQUEST_UPDATE_ADN_RECORD;
                break;

            default:
                throw new RuntimeException("Unrecognized solicited response: " + rr.mRequest);
            }
        }
        if (RILJ_LOGD) riljLog(rr.serialString() + "< " + requestToString(rr.mRequest)
                               + " " + retToString(rr.mRequest, ret));
        if (rr.mResult != null) {
            AsyncResult.forMessage(rr.mResult, ret, null);
            rr.mResult.sendToTarget();
        }

        return rr;
    }

    protected void processUnsolicited (Parcel p, int type) {
        Object ret;
        int dataPosition = p.dataPosition(); // save off position within the Parcel
        int response = p.readInt();

        switch(response) {
        case RIL_UNSOL_RESPONSE_ADN_INIT_DONE_LEGACY:
            ret = responseVoid(p);
            break;
        case RIL_UNSOL_RESPONSE_ADN_RECORDS_LEGACY:
            ret = responseAdnRecords(p);
            break;
        default:
            // Rewind the Parcel
            p.setDataPosition(dataPosition);

            // Forward responses that we are not overriding to the super class
            super.processUnsolicited(p, type);
            return;
        }
    }

    // below is a copy&paste from RIL class, replacing the RIL_REQUEST* with _LEGACY enums
    private Object responseAdnRecords(Parcel p) {
        int numRecords = p.readInt();
        SimPhoneBookAdnRecord[] AdnRecordsInfoGroup = new SimPhoneBookAdnRecord[numRecords];

        for (int i = 0 ; i < numRecords ; i++) {
            AdnRecordsInfoGroup[i]= new SimPhoneBookAdnRecord();

            AdnRecordsInfoGroup[i].mRecordIndex = p.readInt();
            AdnRecordsInfoGroup[i].mAlphaTag = p.readString();
            AdnRecordsInfoGroup[i].mNumber =
                SimPhoneBookAdnRecord.ConvertToPhoneNumber(p.readString());

            int numEmails = p.readInt();
            if(numEmails > 0) {
                AdnRecordsInfoGroup[i].mEmailCount = numEmails;
                AdnRecordsInfoGroup[i].mEmails = new String[numEmails];
                for (int j = 0 ; j < numEmails; j++) {
                    AdnRecordsInfoGroup[i].mEmails[j] = p.readString();
                }
            }

            int numAnrs = p.readInt();
            if(numAnrs > 0) {
                AdnRecordsInfoGroup[i].mAdNumCount = numAnrs;
                AdnRecordsInfoGroup[i].mAdNumbers = new String[numAnrs];
                for (int k = 0 ; k < numAnrs; k++) {
                    AdnRecordsInfoGroup[i].mAdNumbers[k] =
                        SimPhoneBookAdnRecord.ConvertToPhoneNumber(p.readString());
                }
            }
        }
        riljLog(Arrays.toString(AdnRecordsInfoGroup));

        return AdnRecordsInfoGroup;
    }

    public void getAtr(Message response) {
        RILRequest rr = RILRequest.obtain(RIL_REQUEST_SIM_GET_ATR_LEGACY, response);
        int slotId = 0;
        rr.mParcel.writeInt(1);
        rr.mParcel.writeInt(slotId);
        if (RILJ_LOGD) riljLog(rr.serialString() + "> iccGetAtr: "
                               + requestToString(rr.mRequest) + " " + slotId);

        send(rr);
    }

    public void iccOpenLogicalChannel(String AID, byte p2, Message response) {
        RILRequest rr = RILRequest.obtain(RIL_REQUEST_CAF_SIM_OPEN_CHANNEL_WITH_P2_LEGACY, response);
        rr.mParcel.writeByte(p2);
        rr.mParcel.writeString(AID);

        if (RILJ_LOGD)
            riljLog(rr.serialString() + "> " + requestToString(rr.mRequest));

        send(rr);
    }

    public void getAdnRecord(Message result) {
        RILRequest rr = RILRequest.obtain(RIL_REQUEST_GET_ADN_RECORD_LEGACY, result);

        if (RILJ_LOGD) riljLog(rr.serialString() + "> " + requestToString(rr.mRequest));

        send(rr);
    }

    public void updateAdnRecord(SimPhoneBookAdnRecord adnRecordInfo, Message result) {
        RILRequest rr = RILRequest.obtain(RIL_REQUEST_UPDATE_ADN_RECORD_LEGACY, result);
        rr.mParcel.writeInt(adnRecordInfo.getRecordIndex());
        rr.mParcel.writeString(adnRecordInfo.getAlphaTag());
        rr.mParcel.writeString(
                               SimPhoneBookAdnRecord.ConvertToRecordNumber(adnRecordInfo.getNumber()));

        int numEmails = adnRecordInfo.getNumEmails();
        rr.mParcel.writeInt(numEmails);
        for (int i = 0 ; i < numEmails; i++) {
            rr.mParcel.writeString(adnRecordInfo.getEmails()[i]);
        }

        int numAdNumbers = adnRecordInfo.getNumAdNumbers();
        rr.mParcel.writeInt(numAdNumbers);
        for (int j = 0 ; j < numAdNumbers; j++) {
            rr.mParcel.writeString(
                                   SimPhoneBookAdnRecord.ConvertToRecordNumber(adnRecordInfo.getAdNumbers()[j]));
        }

        if (RILJ_LOGD) riljLog(rr.serialString() + "> " + requestToString(rr.mRequest)
                               + " with " + adnRecordInfo.toString());

        send(rr);
    }

    // new ril methods that are not supported
    public void setAllowedCarriers(List<CarrierIdentifier> carriers, Message response) {
        riljLog("setAllowedCarriers: not supported");
        if (response != null) {
            CommandException ex = new CommandException(CommandException.Error.REQUEST_NOT_SUPPORTED);
            AsyncResult.forMessage(response, null, ex);
            response.sendToTarget();
        }
    }

    public void getAllowedCarriers(Message response) {
        riljLog("getAllowedCarriers: not supported");
        if (response != null) {
            CommandException ex = new CommandException(CommandException.Error.REQUEST_NOT_SUPPORTED);
            AsyncResult.forMessage(response, null, ex);
            response.sendToTarget();
        }
    }
}
