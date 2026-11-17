const loading = document.querySelector('#loading');
const quizContent = document.querySelector('#quiz-content');
const questionsNode = document.querySelector('#questions');
const form = document.querySelector('#quiz-form');
const resultNode = document.querySelector('#result');
const errorState = document.querySelector('#error-state');
const messageNode = document.querySelector('#form-message');
let quizQuestions = [];

async function loadQuiz() {
  loading.classList.remove('hidden');
  quizContent.classList.add('hidden');
  resultNode.classList.add('hidden');
  errorState.classList.add('hidden');
  try {
    const response = await fetch('/api/questions');
    if (!response.ok) throw new Error('The quiz server returned an error.');
    const data = await response.json();
    quizQuestions = data.questions;
    renderQuestions();
    loading.classList.add('hidden');
    quizContent.classList.remove('hidden');
    updateProgress();
  } catch (error) {
    loading.classList.add('hidden');
    errorState.classList.remove('hidden');
  }
}

function renderQuestions() {
  const letters = ['A', 'B', 'C', 'D'];
  document.querySelector('#question-count').textContent = `· ${quizQuestions.length} QUESTIONS`;
  questionsNode.innerHTML = quizQuestions.map((question, index) => `
    <fieldset class="question">
      <legend><span class="question-number">${String(index + 1).padStart(2, '0')}</span>${escapeHtml(question.prompt)}</legend>
      <div class="choices">
        ${question.choices.map((choice, choiceIndex) => `
          <label class="choice">
            <input type="radio" name="q-${question.id}" value="${choiceIndex}" aria-label="${letters[choiceIndex]}: ${escapeHtml(choice)}">
            <span class="choice-letter">${letters[choiceIndex]}</span><span>${escapeHtml(choice)}</span>
          </label>`).join('')}
      </div>
    </fieldset>`).join('');
}

function escapeHtml(value) {
  return String(value).replace(/[&<>"']/g, char => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[char]));
}

function updateProgress() {
  const answered = quizQuestions.filter(q => form.querySelector(`input[name="q-${q.id}"]:checked`)).length;
  document.querySelector('#progress-bar').style.width = `${quizQuestions.length ? answered / quizQuestions.length * 100 : 0}%`;
}

form.addEventListener('change', updateProgress);
form.addEventListener('submit', async event => {
  event.preventDefault();
  const answers = quizQuestions.map(q => form.querySelector(`input[name="q-${q.id}"]:checked`));
  if (answers.some(answer => !answer)) {
    messageNode.textContent = 'Pick one answer for each question to see your results.';
    const firstMissing = quizQuestions[answers.findIndex(answer => !answer)];
    form.querySelector(`input[name="q-${firstMissing.id}"]`).focus();
    return;
  }
  messageNode.textContent = '';
  const button = document.querySelector('#submit-btn');
  button.disabled = true;
  button.textContent = 'Checking…';
  try {
    const response = await fetch('/api/submit', {
      method: 'POST', headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({answers: answers.map(answer => Number(answer.value))})
    });
    const data = await response.json();
    if (!response.ok) throw new Error(data.error || 'Could not check your answers.');
    showResults(data);
  } catch (error) {
    messageNode.textContent = error.message;
  } finally {
    button.disabled = false;
    button.innerHTML = 'Check my answers <span aria-hidden="true">↗</span>';
  }
});

function showResults(data) {
  const percent = data.score / data.total;
  const headline = percent === 1 ? 'Perfect score!' : percent >= .6 ? 'Nicely done!' : 'Good effort!';
  const burst = percent === 1 ? '✦' : percent >= .6 ? '✳' : '☀';
  const reviews = data.results.map((item, index) => {
    const q = quizQuestions[index];
    const correct = item.selected === item.correct;
    return `<div class="review-item">
      <div class="review-icon ${correct ? 'good' : 'bad'}">${correct ? '✓' : '↗'}</div>
      <div><div class="review-question">${escapeHtml(q.prompt)}</div>
        <div class="review-detail">${correct ? 'You got it!' : `Your answer: <strong>${escapeHtml(q.choices[item.selected])}</strong> · Correct: <strong>${escapeHtml(q.choices[item.correct])}</strong>`}</div>
        <div class="review-explanation">${escapeHtml(item.explanation)}</div>
      </div>
    </div>`;
  }).join('');
  resultNode.innerHTML = `<div class="result-header"><div class="result-burst">${burst}</div><div class="result-label">QUIZ COMPLETE</div><h2>${headline}</h2><div class="score-line">You got <span class="score-number">${data.score} out of ${data.total}</span> right. Keep that curious mind going.</div></div><div class="review-list">${reviews}</div><div class="result-actions"><button id="play-again" class="submit-button" type="button">Have another go <span aria-hidden="true">↻</span></button></div>`;
  quizContent.classList.add('hidden');
  resultNode.classList.remove('hidden');
  document.querySelector('#play-again').addEventListener('click', () => {
    form.reset(); messageNode.textContent = ''; updateProgress();
    resultNode.classList.add('hidden'); quizContent.classList.remove('hidden');
    window.scrollTo({top: 0, behavior: 'smooth'});
  });
  resultNode.focus?.();
  window.scrollTo({top: 0, behavior: 'smooth'});
}

document.querySelector('#retry-load').addEventListener('click', loadQuiz);
loadQuiz();
