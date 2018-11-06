#include <iostream>
#include "Strategy.h"
#include "HighLow.h"
#include "MA.h"
#include "BarSeries.h"
#include "StrategyConfig.h"
#include "SimulatorConfig.h"
#include "Engine.h"

using namespace AlgoSE;

class PivotReversal : public Strategy
{
public:
    void onCreate()
    {
        m_maFilter = 1;
        m_stopLoss = 0;
        m_stopLossPct = 0.1;
    }

    void onSetParameter(const char* name, int type, const char* value, bool isLast)
    {
        if (!_stricmp(name, "LEStrength")) {
            m_LEStrength = atoi(value);
        } else if (!_stricmp(name, "SEStrength")) {
            m_SEStrength = atoi(value);
        } else if (!_stricmp(name, "shortMA")) {
            m_shortPeriod = atoi(value);
        } else if (!_stricmp(name, "longMA")) {
            m_longPeriod = atoi(value);
        } else if (!_stricmp(name, "MAFilter")) {
            m_maFilter = atoi(value);
        } else if (!_stricmp(name, "stopLoss")) {
            m_stopLoss = atoi(value);
        } else if (!_stricmp(name, "stopLossPct")) {
            m_stopLossPct = atof(value);
        }
    }

    void onStart()
    {
        m_swingHi.hasVal = false;
        m_swingLo.hasVal = false;
        m_swingHi.swingVal = 0.0;
        m_swingLo.swingVal = 0.0;
        m_longEma.init(getBarSeries()->getCloseDataSeries(), m_longPeriod);
        m_shortEma.init(getBarSeries()->getCloseDataSeries(), m_shortPeriod);
        m_dateNum = 0;
        m_tradesNum = 0;

        enablePaperTrading();
    }

    void onTick(const Tick& tick)
    {
        if (m_swingHi.hasVal && tick.lastPrice > m_swingHi.swingVal) {
            closeShort();

            if (m_maFilter) {
                if (m_direction == 1) {
                    openLong();
                }
            } else {
                openLong();
            }
        }

        if (m_swingLo.hasVal && tick.lastPrice < m_swingLo.swingVal) {
            closeLong();
            if (m_maFilter) {
                if (m_direction == -1) {
                    openShort();
                }
            } else {
                openShort();
            }
        }

        if (m_stopLoss) {
            checkStopLoss(tick);
        }

        if (isVerbose()) {
            char verbose[128] = { 0 };
            if (m_swingHi.hasVal) {
                sprintf(verbose, "SwingHi: %.02f @ %d %d", m_swingHi.swingVal, m_swingHi.date, m_swingHi.time);
            }
            if (m_swingLo.hasVal) {
                int len = strlen(verbose);
                verbose[len] = ' ';
                sprintf(verbose + len + 1, "SwingLo: %.02f @ %d %d", m_swingLo.swingVal, m_swingLo.date, m_swingLo.time);
            }
            writeVerboseMsg(verbose);
        }
    }

    void onBar(const Bar& bar)
    {
        if (m_maFilter) {
            onBar_MAFilter(bar);
        } else {
            onBar_Simple(bar);
        }
    }

private:
    void onBar_MAFilter(const Bar& bar)
    {
        DateTime dt(bar.timestamp);

        int dateNum = dt.dateNum();
        int timeNum = dt.timeNum();
        if (dateNum != m_dateNum) {
            m_newTradingDay = true;
            m_tradesNum = 0;
        } else {
            m_newTradingDay = false;
        }

        m_dateNum = dateNum;

        if (m_longEma.length() < m_longPeriod || m_shortEma.length() < m_shortPeriod) {
            m_direction = 0;
        } else {
            if (m_shortEma[1] > m_longEma[1]) {
                m_direction = 1;
            } else if (m_shortEma[1] < m_longEma[1]) {
                m_direction = -1;
            }
        }

        SequenceDataSeries<double>& high = (SequenceDataSeries<double>&)getBarSeries()->getHighDataSeries();
        if (SwingHigh(high, m_LEStrength, m_LEStrength)) {
            m_swingHi.swingVal = high[m_LEStrength];
            m_swingHi.hasVal = true;
            m_swingHi.date = dateNum;
            m_swingHi.time = timeNum;
            if (isVerbose()) {
                char verbose[128] = { 0 };
                sprintf(verbose, "[%s]: SwingHi: %.02f @ %s", getName(), m_swingHi.swingVal, DateTime(getBarSeries()->at(m_LEStrength).timestamp).toString().c_str());
                writeVerboseMsg(verbose);
            }
        }

        SequenceDataSeries<double>& low = (SequenceDataSeries<double>&)getBarSeries()->getLowDataSeries();
        if (SwingLow(low, m_SEStrength, m_SEStrength)) {
            m_swingLo.swingVal = low[m_SEStrength];
            m_swingLo.hasVal = true;
            m_swingLo.date = dateNum;
            m_swingLo.time = timeNum;
            if (isVerbose()) {
                char verbose[128] = { 0 };
                sprintf(verbose, "[%s]: SwingLo: %.02f @ %s", getName(), m_swingLo.swingVal, DateTime(getBarSeries()->at(m_SEStrength).timestamp).toString().c_str());
                writeVerboseMsg(verbose);
            }
        }

        if (getStatus() == Strategy::LOADING) {
            if (m_swingHi.hasVal && bar.high > m_swingHi.swingVal) {
                if (m_tradesNum < 300 && m_direction == 1) {
                    openLong();
                    m_tradesNum++;
                } else {
                    closeShort();
                }
            }

            if (m_swingLo.hasVal && bar.low < m_swingLo.swingVal) {
                if (m_tradesNum < 300 && m_direction == -1) {
                    openShort();
                    m_tradesNum++;
                } else {
                    closeLong();
                }
            }
        }

        if (m_swingHi.hasVal && bar.high > m_swingHi.swingVal) {
            m_swingHi.hasVal = false;
        }

        if (m_swingLo.hasVal && bar.low < m_swingLo.swingVal) {
            m_swingLo.hasVal = false;
        }
    }

    void onBar_Simple(const Bar& bar)
    {
        SequenceDataSeries<double>& high = (SequenceDataSeries<double>&)getBarSeries()->getHighDataSeries();
        if (SwingHigh(high, m_LEStrength, m_LEStrength)) {
            m_swingHi.swingVal = high[m_LEStrength];
            m_swingHi.hasVal = true;
        }

        SequenceDataSeries<double>& low = (SequenceDataSeries<double>&)getBarSeries()->getLowDataSeries();
        if (SwingLow(low, m_SEStrength, m_SEStrength)) {
            m_swingLo.swingVal = low[m_SEStrength];
            m_swingLo.hasVal = true;
        }

        if (m_swingHi.hasVal && bar.high > m_swingHi.swingVal) {
            openLong();
        }
        
        if (m_swingLo.hasVal && bar.low < m_swingLo.swingVal) {
             openShort();
        }

        if (m_swingHi.hasVal && bar.high > m_swingHi.swingVal) {
            m_swingHi.hasVal = false;
        }

        if (m_swingLo.hasVal && bar.low < m_swingLo.swingVal) {
            m_swingLo.hasVal = false;
        }
    }

    void checkStopLoss(const Tick& tick)
    {
        StrategyPosition pos = getLongPos(tick.instrument);
        if (pos.quantity > 0) {
            double loss = tick.lastPrice - pos.avgPx;
            if (loss < 0) {
                double pct = fabs(loss) * getMultiplier(tick.instrument) / pos.avgPx;
                if (pct > m_stopLossPct) {
                    closeLong();
                }
            }
        }

        pos = getShortPos(tick.instrument);
        if (pos.quantity > 0) {
            double loss = pos.avgPx - tick.lastPrice;
            if (loss < 0) {
                double pct = fabs(loss) * getMultiplier(tick.instrument) / pos.avgPx;
                if (pct > m_stopLossPct) {
                    closeShort();
                }
            }
        }
    }
    
private:
    typedef struct _SwingValue {
        int date;
        int time;
        bool hasVal;
        double swingVal;
        _SwingValue() {
            date = 0;
            time = 0;
            hasVal = false;
        }
    } SwingValue;

    SwingValue m_swingHi;
    SwingValue m_swingLo;

    int m_LEStrength;
    int m_SEStrength;

    int m_maFilter;

    EMA m_longEma;
    EMA m_shortEma;

    int m_shortPeriod;
    int m_longPeriod;

    int m_direction;
    bool m_newTradingDay;
    int m_dateNum;
    int m_tradesNum;

    int m_stopLoss;
    double m_stopLossPct;
};

EXPORT_STRATEGY(PivotReversal)
