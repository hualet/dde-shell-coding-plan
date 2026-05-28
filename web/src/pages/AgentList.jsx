import React, { useEffect, useState } from 'react';
import { useNavigate } from 'react-router-dom';
import {
  Box, Card, CardContent, Typography, Chip, CircularProgress, List, ListItem, IconButton, Stack
} from '@mui/material';
import CheckCircleIcon from '@mui/icons-material/CheckCircle';
import ErrorOutlineIcon from '@mui/icons-material/ErrorOutline';
import OpenInNewIcon from '@mui/icons-material/OpenInNew';
import RefreshIcon from '@mui/icons-material/Refresh';
import { fetchProviders, fetchSnapshots, isLoggedIn, refreshAll, openConsole, onDataChanged } from '../bridge';

const severityColor = {
  normal: 'success',
  warning: 'warning',
  critical: 'error',
  error: 'default',
};

function AgentCard({ provider, snapshot, onNavigate }) {
  const loggedIn = isLoggedIn(snapshot);
  const navigate = useNavigate();

  const handleClick = () => {
    if (loggedIn) {
      navigate(`/quota/${provider.id}`);
    } else {
      navigate(`/login/${provider.id}`);
    }
  };

  const weeklyPercent = snapshot && snapshot.remainingRatio >= 0
    ? Math.round(snapshot.remainingRatio * 100) : null;

  const fiveHourPercent = snapshot && snapshot.fiveHourRemainingRatio >= 0
    ? Math.round(snapshot.fiveHourRemainingRatio * 100)
    : null;

  const barColor = snapshot?.severity === 'normal' ? 'success.main'
    : snapshot?.severity === 'warning' ? 'warning.main'
    : snapshot?.severity === 'critical' ? 'error.main'
    : 'grey.400';

  return (
    <Card
      onClick={handleClick}
      sx={{
        mb: 1.5,
        cursor: 'pointer',
        transition: 'transform 0.15s, box-shadow 0.15s',
        '&:active': {
          transform: 'scale(0.98)',
          boxShadow: '0 1px 6px rgba(0,0,0,0.12)',
        },
      }}
    >
      <CardContent sx={{ py: 2, '&:last-child': { pb: 2 } }}>
        <Stack direction="row" alignItems="center" justifyContent="space-between">
          <Box sx={{ flex: 1, minWidth: 0 }}>
            <Stack direction="row" alignItems="center" spacing={1}>
              <Typography variant="subtitle1" fontWeight={600} noWrap>
                {provider.name}
              </Typography>
              <Chip
                icon={loggedIn ? <CheckCircleIcon /> : <ErrorOutlineIcon />}
                label={loggedIn ? '已登录' : '未登录'}
                size="small"
                color={loggedIn ? 'success' : 'default'}
                variant={loggedIn ? 'filled' : 'outlined'}
                sx={{ height: 24, fontSize: '0.75rem' }}
              />
            </Stack>
            {!loggedIn && snapshot && (
              <Typography variant="body2" color="text.secondary" sx={{ mt: 0.5 }}>
                {snapshot.message || '需要登录'}
              </Typography>
            )}
          </Box>
          <Stack direction="row" spacing={0.5}>
            <IconButton
              size="small"
              onClick={(e) => {
                e.stopPropagation();
                openConsole(provider.id);
              }}
            >
              <OpenInNewIcon fontSize="small" />
            </IconButton>
          </Stack>
        </Stack>
        {fiveHourPercent !== null && (
          <Box sx={{ mt: 1 }}>
            <Stack direction="row" justifyContent="space-between" sx={{ mb: 0.3 }}>
              <Typography variant="body2" color="text.secondary">5小时额度</Typography>
              <Typography variant="body2" fontWeight={600}>{fiveHourPercent}%</Typography>
            </Stack>
            <Box sx={{ width: '100%', height: 4, borderRadius: 2, bgcolor: 'grey.200' }}>
              <Box sx={{ height: '100%', borderRadius: 2, width: `${fiveHourPercent}%`, bgcolor: barColor, transition: 'width 0.3s' }} />
            </Box>
          </Box>
        )}
        {weeklyPercent !== null && (
          <Box sx={{ mt: 0.8 }}>
            <Stack direction="row" justifyContent="space-between" sx={{ mb: 0.3 }}>
              <Typography variant="body2" color="text.secondary">周额度</Typography>
              <Typography variant="body2" fontWeight={600}>{weeklyPercent}%</Typography>
            </Stack>
            <Box sx={{ width: '100%', height: 4, borderRadius: 2, bgcolor: 'grey.200' }}>
              <Box sx={{ height: '100%', borderRadius: 2, width: `${weeklyPercent}%`, bgcolor: barColor, transition: 'width 0.3s' }} />
            </Box>
          </Box>
        )}
      </CardContent>
    </Card>
  );
}

export default function AgentList() {
  const [providers, setProviders] = useState([]);
  const [snapshots, setSnapshots] = useState([]);
  const [loading, setLoading] = useState(true);
  const navigate = useNavigate();

  const loadData = async () => {
    setLoading(true);
    const [p, s] = await Promise.all([fetchProviders(), fetchSnapshots()]);
    setProviders(p || []);
    setSnapshots(s || []);
    setLoading(false);
  };

  useEffect(() => {
    loadData();
    const unsub = onDataChanged(() => {
      loadData();
    });
    return () => {
      if (typeof unsub === 'function') unsub();
    };
  }, []);

  const handleRefreshAll = async () => {
    await refreshAll();
    setTimeout(loadData, 500);
  };

  const getSnapshot = (providerId) => {
    return snapshots.find((s) => s.providerId === providerId) || null;
  };

  return (
    <Box sx={{ p: 2 }}>
      <Stack direction="row" justifyContent="space-between" alignItems="center" sx={{ mb: 2 }}>
        <Typography variant="h5">
          AI Coding Plan
        </Typography>
        <IconButton onClick={handleRefreshAll} disabled={loading}>
          <RefreshIcon />
        </IconButton>
      </Stack>

      {loading && providers.length === 0 ? (
        <Box sx={{ display: 'flex', justifyContent: 'center', py: 8 }}>
          <CircularProgress />
        </Box>
      ) : (
        <List disablePadding>
          {providers.map((provider) => (
            <ListItem key={provider.id} disablePadding sx={{ mb: 0 }}>
              <AgentCard
                provider={provider}
                snapshot={getSnapshot(provider.id)}
                onNavigate={(id, loggedIn) => {
                  navigate(loggedIn ? `/quota/${id}` : `/login/${id}`);
                }}
              />
            </ListItem>
          ))}
        </List>
      )}

      {providers.length === 0 && !loading && (
        <Box sx={{ textAlign: 'center', py: 8 }}>
          <Typography color="text.secondary">
            暂无 Coding Plan 服务
          </Typography>
        </Box>
      )}
    </Box>
  );
}
