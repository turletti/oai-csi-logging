#!/bin/bash
set -e
OAI_DIR="${OAI_DIR:-/oai-ran}"
RRC_UE_FILE="$OAI_DIR/openair2/RRC/NR_UE/rrc_UE.c"

if [ ! -f "$RRC_UE_FILE" ]; then
  echo "❌ rrc_UE.c not found at $RRC_UE_FILE"
  exit 1
fi

if ! grep -q "maxNumberSRS_Ports_PerResource = NR_SRS_Resources__maxNumberSRS_Ports_PerResource_n2" "$RRC_UE_FILE"; then
  sed -i '/nr_bandnr->bandNR = 1;/a\  /* Add featureSets with SRS resources capability (2 ports for MIMO) */\n  rrc->UECap.UE_NR_Capability->featureSets = CALLOC(1, sizeof(NR_FeatureSets_t));\n  rrc->UECap.UE_NR_Capability->featureSets->featureSetsUplink = CALLOC(1, sizeof(struct NR_FeatureSets__featureSetsUplink));\n  NR_FeatureSetUplink_t *ul_feature_set;\n  asn1cSequenceAdd(rrc->UECap.UE_NR_Capability->featureSets->featureSetsUplink->list, NR_FeatureSetUplink_t, ul_feature_set);\n  ul_feature_set->supportedSRS_Resources = CALLOC(1, sizeof(NR_SRS_Resources_t));\n  ul_feature_set->supportedSRS_Resources->maxNumberSRS_Ports_PerResource = NR_SRS_Resources__maxNumberSRS_Ports_PerResource_n2;\n  ul_feature_set->supportedSRS_Resources->maxNumberAperiodicSRS_PerBWP = NR_SRS_Resources__maxNumberAperiodicSRS_PerBWP_n16;\n  ul_feature_set->supportedSRS_Resources->maxNumberAperiodicSRS_PerBWP_PerSlot = 6;\n  ul_feature_set->supportedSRS_Resources->maxNumberPeriodicSRS_PerBWP = NR_SRS_Resources__maxNumberPeriodicSRS_PerBWP_n16;\n  ul_feature_set->supportedSRS_Resources->maxNumberPeriodicSRS_PerBWP_PerSlot = 6;\n  ul_feature_set->supportedSRS_Resources->maxNumberSemiPersistentSRS_PerBWP = NR_SRS_Resources__maxNumberSemiPersistentSRS_PerBWP_n16;\n  ul_feature_set->supportedSRS_Resources->maxNumberSemiPersistentSRS_PerBWP_PerSlot = 4;' "$RRC_UE_FILE"
  echo "✅ UE SRS capability patched (2 ports)"
else
  echo "✅ UE SRS capability already patched"
fi
