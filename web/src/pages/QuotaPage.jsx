import React, { useEffect, useState } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import {
  Box, Typography, Card, CardContent, Button, Stack, Chip, IconButton, CircularProgress
} from '@mui/material';
import CheckCircleIcon from '@mui/icons-material/CheckCircle';
import RefreshIcon from '@mui/icons-material/Refresh';
import OpenInNewIcon from '@mui/icons-material/OpenInNew';
import { fetchProviders, fetchSnapshots, refreshProvider, openConsole } from '../bridge';

export default function QuotaPage() {
  const { providerId } = useParams();
  const navigate = useNavigate();
  const [provider, setProvider] = useState(null);
  const [snapshot, setSnapshot] = useState(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    async function load() {
      const [providers, snapshots] = await Promise.all([
        fetchProviders(),
        fetchSnapshots(),
      ]);
      const p = (providers || []).find((x) => x.id === providerId);
      const s = (snapshots || []).find((x) => x.providerId === providerId);
      setProvider(p || null);
      setSnapshot(s || null);
      setLoading(false);
    }
    load();
  }, [providerId]);

  const handleRefresh = async () => {
    setLoading(true);
    await refreshProvider(providerId);
    const [, snapshots] = await Promise.all([
      fetchProviders(),
      fetchSnapshots(),
    ]);
    const s = (snapshots || []).find((x) => x.providerId === providerId);
    setSnapshot(s || null);
    setLoading(false);
  };

  if (loading) {
    return (
      <Box sx={{ display: 'flex', justifyContent: 'center', py: 8 }}>
        <CircularProgress />
      </Box>
    );
  }

  if (!provider) {
    return (
      <Box sx={{ p: 3, textAlign: 'center' }}>
        <Typography color="error">未找到该服务</Typography>
        <Button sx={{ mt: 2 }} onClick={() => navigate('/')}>返回</Button>
      </Box>
    );
  }

  const weeklyPercent = snapshot && snapshot.remainingRatio >= 0
    ? Math.round(snapshot.remainingRatio * 100) : null;

  const fiveHourPercent = snapshot && snapshot.fiveHourRemainingRatio >= 0
    ? Math.round(snapshot.fiveHourRemainingRatio * 100)
    : null;

  const statusLabel = snapshot?.status === 'ok' ? '正常'
    : snapshot?.status === 'warning' ? '预警'
    : snapshot?.status === 'exhausted' ? '已耗尽'
    : snapshot?.status === 'auth_error' ? '认证失败'
    : snapshot?.status === 'parse_error' ? '读取失败'
    : snapshot?.status === 'network_error' ? '网络错误'
    : '未知';

  const statusColor = snapshot?.status === 'ok' ? 'success'
    : snapshot?.status === 'warning' ? 'warning'
    : ['exhausted', 'auth_error', 'parse_error', 'network_error'].includes(snapshot?.status) ? 'error'
    : 'default';

  return (
    <Box sx={{ p: 2 }}>
      <Card sx={{ mb: 2 }}>
        <CardContent>
          <Stack direction="row" alignItems="center" justifyContent="space-between" sx={{ mb: 2 }}>
            <Typography variant="h6">{provider.name}</Typography>
            <Stack direction="row" spacing={1}>
              <IconButton size="small" onClick={handleRefresh}>
                <RefreshIcon fontSize="small" />
              </IconButton>
              <IconButton size="small" onClick={() => openConsole(providerId)}>
                <OpenInNewIcon fontSize="small" />
              </IconButton>
            </Stack>
          </Stack>

          <Chip
            icon={<CheckCircleIcon />}
            label={statusLabel}
            color={statusColor}
            size="small"
            sx={{ mb: 2 }}
          />

          {fiveHourPercent !== null && (
            <Box sx={{ mb: 1.5 }}>
              <Stack direction="row" justifyContent="space-between" sx={{ mb: 0.5 }}>
                <Typography variant="body2" color="text.secondary">5小时额度</Typography>
                <Typography variant="body2" fontWeight={600}>{fiveHourPercent}%</Typography>
              </Stack>
              <Box sx={{ width: '100%', height: 10, borderRadius: 5, bgcolor: 'grey.200' }}>
                <Box
                  sx={{
                    height: '100%',
                    borderRadius: 5,
                    width: `${fiveHourPercent}%`,
                    bgcolor: snapshot?.severity === 'normal' ? 'success.main'
                      : snapshot?.severity === 'warning' ? 'warning.main'
                      : snapshot?.severity === 'critical' ? 'error.main'
                      : 'grey.400',
                    transition: 'width 0.3s',
                  }}
                />
              </Box>
            </Box>
          )}

          {weeklyPercent !== null && (
            <Box sx={{ mb: 1.5 }}>
              <Stack direction="row" justifyContent="space-between" sx={{ mb: 0.5 }}>
                <Typography variant="body2" color="text.secondary">周额度</Typography>
                <Typography variant="body2" fontWeight={600}>{weeklyPercent}%</Typography>
              </Stack>
              <Box sx={{ width: '100%', height: 10, borderRadius: 5, bgcolor: 'grey.200' }}>
                <Box
                  sx={{
                    height: '100%',
                    borderRadius: 5,
                    width: `${weeklyPercent}%`,
                    bgcolor: snapshot?.severity === 'normal' ? 'success.main'
                      : snapshot?.severity === 'warning' ? 'warning.main'
                      : snapshot?.severity === 'critical' ? 'error.main'
                      : 'grey.400',
                    transition: 'width 0.3s',
                  }}
                />
              </Box>
            </Box>
          )}

          {snapshot?.fiveHourBalanceText && (
            <Typography variant="body2" color="text.secondary" sx={{ mb: 0.5 }}>
              5小时额度余额: {snapshot.fiveHourBalanceText}
            </Typography>
          )}

          {snapshot?.balanceText && (
            <Typography variant="body2" color="text.secondary" sx={{ mb: 1 }}>
              周额度余额: {snapshot.balanceText}
            </Typography>
          )}

          {snapshot?.updatedAt && (
            <Typography variant="caption" color="text.secondary">
              更新于 {new Date(snapshot.updatedAt).toLocaleString('zh-CN')}
            </Typography>
          )}

          {snapshot?.message && (
            <Typography variant="body2" color="text.secondary" sx={{ mt: 1 }}>
              {snapshot.message}
            </Typography>
          )}
        </CardContent>
      </Card>

      <Stack spacing={1}>
        <Button
          variant="outlined"
          fullWidth
          startIcon={<RefreshIcon />}
          onClick={handleRefresh}
        >
          刷新额度
        </Button>
        <Button
          variant="outlined"
          fullWidth
          startIcon={<OpenInNewIcon />}
          onClick={() => openConsole(providerId)}
        >
          打开控制台
        </Button>
      </Stack>
    </Box>
  );
}
