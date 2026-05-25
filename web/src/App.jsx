import React from 'react';
import { ThemeProvider } from '@mui/material/styles';
import { CssBaseline, AppBar, Toolbar, Typography, Box } from '@mui/material';
import { MemoryRouter, Routes, Route, useNavigate, useLocation } from 'react-router-dom';
import theme from './theme';
import AgentList from './pages/AgentList';
import QuotaPage from './pages/QuotaPage';
import LoginPage from './pages/LoginPage';
import { SlideTransition } from './components/SlideTransition';

function PageWrapper({ children }) {
  const navigate = useNavigate();
  const location = useLocation();
  const isRoot = location.pathname === '/';

  return (
    <Box sx={{ display: 'flex', flexDirection: 'column', height: '100vh', bgcolor: 'background.default' }}>
      <AppBar position="static" elevation={0} sx={{ bgcolor: 'primary.main' }}>
        <Toolbar variant="dense">
          {!isRoot && (
            <Typography
              variant="body2"
              sx={{ mr: 1, cursor: 'pointer', color: 'inherit', textDecoration: 'none' }}
              onClick={() => navigate('/')}
            >
              ‹ 返回
            </Typography>
          )}
          <Typography variant="h6" sx={{ flexGrow: 1, fontSize: '1.1rem' }}>
            Coding Plan
          </Typography>
        </Toolbar>
      </AppBar>
      <Box sx={{ flex: 1, overflow: 'auto' }}>
        {children}
      </Box>
    </Box>
  );
}

function AppRoutes() {
  return (
    <PageWrapper>
      <Routes>
        <Route path="/" element={<AgentList />} />
        <Route path="/quota/:providerId" element={<QuotaPage />} />
        <Route path="/login/:providerId" element={<LoginPage />} />
      </Routes>
    </PageWrapper>
  );
}

export default function App() {
  return (
    <ThemeProvider theme={theme}>
      <CssBaseline />
      <MemoryRouter initialEntries={['/']}>
        <SlideTransition>
          <AppRoutes />
        </SlideTransition>
      </MemoryRouter>
    </ThemeProvider>
  );
}
