// SPDX-License-Identifier: GPL-2.0
/******************************************************************************
 *
 * Copyright(c) 2009-2010  Realtek Corporation.
 *
 *****************************************************************************/

#include <drv_types.h>
#include <rtw_debug.h>

#include <rtw_wifi_regd.h>

/*
 * REG_RULE(freq start, freq end, bandwidth, max gain, eirp, reg_flags)
 */

/*
 * Only these channels all allow active
 * scan on all world regulatory domains
 */

/* 2G chan 01 - chan 11 */
#define RTW_2GHZ_CH01_11	\
	REG_RULE(2410 - 10, 2484 + 10, 40, 0, 25, 0)

/*
 * We enable active scan on these a case
 * by case basis by regulatory domain
 */

/* 2G chan 12 - chan 13, PASSIV SCAN */
#define RTW_2GHZ_CH12_13	\
	REG_RULE(2467 - 10, 2472 + 10, 40, 0, 25,	\
	NL80211_RRF_PASSIVE_SCAN)

/* 2G chan 14, PASSIVS SCAN, NO OFDM (B only) */
#define RTW_2GHZ_CH14	\
	REG_RULE(2484 - 10, 2484 + 10, 40, 0, 25,	\
	NL80211_RRF_PASSIVE_SCAN | NL80211_RRF_NO_OFDM)

static const struct ieee80211_regdomain rtw_regdom_rd = {
	.n_reg_rules = 1,
	.alpha2 = "03",
	.reg_rules = {
		RTW_2GHZ_CH01_11,
	}
};

static bool _rtl_is_radar_freq(u16 center_freq)
{
	return center_freq >= 5260 && center_freq <= 5700;
}

enum ieee80211_band {
	IEEE80211_BAND_2GHZ = NL80211_BAND_2GHZ,
	IEEE80211_BAND_5GHZ = NL80211_BAND_5GHZ,
	IEEE80211_BAND_60GHZ = NL80211_BAND_60GHZ,

	/* keep last */
	IEEE80211_NUM_BANDS
};

static int rtw_ieee80211_channel_to_frequency(int chan, int band)
{
	/* see 802.11 17.3.8.3.2 and Annex J
	 * there are overlapping channel numbers in 5GHz and 2GHz bands
	 */

	/* NL80211_BAND_2GHZ */
	if (chan == 14)
		return 2484;
	else if (chan < 14)
		return 2407 + chan * 5;
	else
		return 0;	/* not supported */
}

static void _rtw_reg_apply_flags(struct wiphy *wiphy)
{
	_adapter *padapter = wiphy_to_adapter(wiphy);
	struct mlme_ext_priv *pmlmeext = &padapter->mlmeextpriv;
	RT_CHANNEL_INFO *channel_set = pmlmeext->channel_set;
	u8 max_chan_nums = pmlmeext->max_chan_nums;

	struct ieee80211_supported_band *sband;
	struct ieee80211_channel *ch;
	unsigned int i, j;
	u16 channel;
	u32 freq;

	/* all channels disable */
	for (i = 0; i < NUM_NL80211_BANDS; i++) {
		sband = wiphy->bands[i];

		if (sband) {
			for (j = 0; j < sband->n_channels; j++) {
				ch = &sband->channels[j];

				if (ch)
					ch->flags = 0; //IEEE80211_CHAN_DISABLED;
			}
		}
	}

	/* channels apply by channel plans. */
	for (i = 0; i < max_chan_nums; i++) {
		channel = channel_set[i].ChannelNum;
		freq =
		    rtw_ieee80211_channel_to_frequency(channel,
						       NL80211_BAND_2GHZ);

		ch = ieee80211_get_channel(wiphy, freq);
		ch->flags = 0;
		
		/*if (ch) {
			if (channel_set[i].ScanType == SCAN_PASSIVE)
				ch->flags = IEEE80211_CHAN_NO_IR;
			else
				ch->flags = 0;
		}*/
	}
}

static int _rtw_reg_notifier_apply(struct wiphy *wiphy,
				   struct regulatory_request *request,
				   struct rtw_regulatory *reg)
{
	/* Hard code flags */
	_rtw_reg_apply_flags(wiphy);
	return 0;
}

static const struct ieee80211_regdomain *_rtw_regdomain_select(struct
							       rtw_regulatory
							       *reg)
{
	return &rtw_regdom_rd;
}


void rtw_reg_notifier(struct wiphy *wiphy, struct regulatory_request *request)
{
	struct rtw_regulatory *reg = NULL;

	DBG_8192C("%s\n", __func__);

	_rtw_reg_notifier_apply(wiphy, request, reg);
}

static void _rtl_reg_apply_radar_flags(struct wiphy *wiphy)
{
	struct ieee80211_supported_band *sband;
	struct ieee80211_channel *ch;
	unsigned int i;

	if (!wiphy->bands[NL80211_BAND_5GHZ])
		return;

	sband = wiphy->bands[NL80211_BAND_5GHZ];

	for (i = 0; i < sband->n_channels; i++) {
		ch = &sband->channels[i];
		if (!_rtl_is_radar_freq(ch->center_freq))
			continue;

		/*
		 *We always enable radar detection/DFS on this
		 *frequency range. Additionally we also apply on
		 *this frequency range:
		 *- If STA mode does not yet have DFS supports disable
		 * active scanning
		 *- If adhoc mode does not support DFS yet then disable
		 * adhoc in the frequency.
		 *- If AP mode does not yet support radar detection/DFS
		 *do not allow AP mode
		 */
		if (!(ch->flags & IEEE80211_CHAN_DISABLED))
			ch->flags |= IEEE80211_CHAN_RADAR |
			    RTW_IEEE80211_CHAN_NO_IBSS |
			    RTW_IEEE80211_CHAN_PASSIVE_SCAN;
	}
}

static void _rtl_reg_apply_beaconing_flags(struct wiphy *wiphy,
					   enum nl80211_reg_initiator initiator)
{
	enum ieee80211_band band;
	struct ieee80211_supported_band *sband;
	const struct ieee80211_reg_rule *reg_rule;
	struct ieee80211_channel *ch;
	unsigned int i;

	for (band = 0; band < IEEE80211_NUM_BANDS; band++) {

		if (!wiphy->bands[band])
			continue;

		sband = wiphy->bands[band];

		for (i = 0; i < sband->n_channels; i++) {
			ch = &sband->channels[i];
			if (_rtl_is_radar_freq(ch->center_freq) ||
			    (ch->flags & IEEE80211_CHAN_RADAR))
				continue;
			if (initiator == NL80211_REGDOM_SET_BY_COUNTRY_IE) {
				reg_rule = freq_reg_info(wiphy,
							 ch->center_freq);
				if (IS_ERR(reg_rule))
					continue;
				/*
				 *If 11d had a rule for this channel ensure
				 *we enable adhoc/beaconing if it allows us to
				 *use it. Note that we would have disabled it
				 *by applying our static world regdomain by
				 *default during init, prior to calling our
				 *regulatory_hint().
				 */

				if (!(reg_rule->flags & NL80211_RRF_NO_IBSS))
					ch->flags &= ~RTW_IEEE80211_CHAN_NO_IBSS;
				if (!(reg_rule->flags &
				      NL80211_RRF_PASSIVE_SCAN))
					ch->flags &=
					    ~RTW_IEEE80211_CHAN_PASSIVE_SCAN;
			} else {
				if (ch->beacon_found)
					ch->flags &= ~(RTW_IEEE80211_CHAN_NO_IBSS |
						   RTW_IEEE80211_CHAN_PASSIVE_SCAN);
			}
		}
	}
}

/* Allows active scan scan on Ch 12 and 13 */
static void _rtl_reg_apply_active_scan_flags(struct wiphy *wiphy,
					     enum nl80211_reg_initiator
					     initiator)
{
	struct ieee80211_supported_band *sband;
	struct ieee80211_channel *ch;
	const struct ieee80211_reg_rule *reg_rule;

	if (!wiphy->bands[NL80211_BAND_2GHZ])
		return;
	sband = wiphy->bands[NL80211_BAND_2GHZ];

	/*
	 *If no country IE has been received always enable active scan
	 *on these channels. This is only done for specific regulatory SKUs
	 */
	if (initiator != NL80211_REGDOM_SET_BY_COUNTRY_IE) {
		ch = &sband->channels[11];	/* CH 12 */
		if (ch->flags & RTW_IEEE80211_CHAN_PASSIVE_SCAN)
			ch->flags &= ~RTW_IEEE80211_CHAN_PASSIVE_SCAN;
		ch = &sband->channels[12];	/* CH 13 */
		if (ch->flags & RTW_IEEE80211_CHAN_PASSIVE_SCAN)
			ch->flags &= ~RTW_IEEE80211_CHAN_PASSIVE_SCAN;
		return;
	}

	/*
	 *If a country IE has been recieved check its rule for this
	 *channel first before enabling active scan. The passive scan
	 *would have been enforced by the initial processing of our
	 *custom regulatory domain.
	 */

	ch = &sband->channels[11];	/* CH 12 */
	reg_rule = freq_reg_info(wiphy, ch->center_freq);
	if (!IS_ERR(reg_rule)) {
		if (!(reg_rule->flags & NL80211_RRF_PASSIVE_SCAN))
			if (ch->flags & RTW_IEEE80211_CHAN_PASSIVE_SCAN)
				ch->flags &= ~RTW_IEEE80211_CHAN_PASSIVE_SCAN;
	}

	ch = &sband->channels[12];	/* CH 13 */
	reg_rule = freq_reg_info(wiphy, ch->center_freq);
	if (!IS_ERR(reg_rule)) {
		if (!(reg_rule->flags & NL80211_RRF_PASSIVE_SCAN))
			if (ch->flags & RTW_IEEE80211_CHAN_PASSIVE_SCAN)
				ch->flags &= ~RTW_IEEE80211_CHAN_PASSIVE_SCAN;
	}
}

static void _rtl_reg_apply_world_flags(struct wiphy *wiphy,
				       enum nl80211_reg_initiator initiator,
				       struct rtw_regulatory *reg)
{
	_rtl_reg_apply_beaconing_flags(wiphy, initiator);
	_rtl_reg_apply_active_scan_flags(wiphy, initiator);
	return;
}

static void _rtw_regd_init_wiphy(struct rtw_regulatory *reg, struct wiphy *wiphy)
{
	const struct ieee80211_regdomain *regd;

	wiphy->reg_notifier = rtw_reg_notifier;

	wiphy->regulatory_flags |= REGULATORY_CUSTOM_REG;
	wiphy->regulatory_flags &= ~REGULATORY_STRICT_REG;
	wiphy->regulatory_flags &= ~REGULATORY_DISABLE_BEACON_HINTS;

	regd = _rtw_regdomain_select(reg);
	wiphy_apply_custom_regulatory(wiphy, regd);
	
	//_rtl_reg_apply_radar_flags(wiphy);
	//_rtl_reg_apply_world_flags(wiphy, NL80211_REGDOM_SET_BY_COUNTRY_IE, NULL);

	/* Hard code flags */
	_rtw_reg_apply_flags(wiphy);
}

void rtw_regd_init(struct wiphy *wiphy)
{
	_rtw_regd_init_wiphy(NULL, wiphy);
}

void rtw_reg_notify_by_driver(struct wiphy *wiphy)
{
	if (wiphy) {
		struct regulatory_request request;
		request.initiator = NL80211_REGDOM_SET_BY_DRIVER;
		rtw_reg_notifier(wiphy, &request);
	}
}



