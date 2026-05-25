import React, { useEffect, useState } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import {
  Box, Typography, Card, CardContent, Button, Stack, CircularProgress, Alert
} from '@mui/material';
import LoginIcon from '@mui/icons-material/Login';
import OpenInNewIcon from '@mui/icons-material/OpenInNew';
import { fetchProviders, requestLogin, openConsole } from '../bridge';

export default function LoginPage() {
  const { providerId } = useParams();
  const navigate = useNavigate();
  const [provider, setProvider] = useState(null);
  const [loading, setLoading] = useState(true);
  const [logging, setLogging] = useState(false);

  useEffect(() => {
    async function load() {
      const providers = await fetchProviders();
      const p = (providers || []).find((x) => x.id === providerId);
      setProvider(p || null);
      setLoading(false);
    }
    load();
  }, [providerId]);

  const handleLogin = async () => {
    setLogging(true);
    await requestLogin(providerId);
    setLogging(false);
  };

  const handleOpenConsole = () => {
    openConsole(providerId);
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

  return (
    <Box sx={{ p: 2 }}>
      <Card sx={{ mb: 2 }}>
        <CardContent>
          <Typography variant="h6" sx={{ mb: 1 }}>
            {provider.name}
          </Typography>
          <Typography variant="body2" color="text.secondary" sx={{ mb: 2 }}>
            需要在官方网站登录后才能读取额度信息
          </Typography>

          <Alert severity="info" sx={{ mb: 2 }}>
            点击下方按钮将跳转到官方登录页面。完成登录后，应用会自动返回并刷新额度。
          </Alert>

          <Typography variant="caption" color="text.secondary" sx={{ mb: 2, display: 'block' }}>
            登录地址: {provider.loginUrl}
          </Typography>
        </CardContent>
      </Card>

      <Stack spacing={1.5}>
        <Button
          variant="contained"
          fullWidth
          size="large"
          startIcon={logging ? <CircularProgress size={20} color="inherit" /> : <LoginIcon />}
          onClick={handleLogin}
          disabled={logging}
          sx={{ py: 1.5 }}
        >
          {logging ? '正在跳转...' : '前往登录'}
        </Button>

        <Button
          variant="outlined"
          fullWidth
          startIcon={<OpenInNewIcon />}
          onClick={handleOpenConsole}
        >
          在浏览器中打开控制台
        </Button>
      </Stack>
    </Box>
  );
}
